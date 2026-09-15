#!/usr/bin/env python3
"""scope.py - ATmega328P MSO host viewer (live window or ASCII fallback).

Reads 260-byte OSC frames (AA 55 + 256 samples + 0D 0A) or 516-byte LA frames
(AA 55 + 512 samples + 0D 0A) streamed by the firmware at 1 Mbps U2X 8N1.
In plot mode (default) a window renders the waveform scrolling in time.

OSC mode: 8-bit ADC samples mapped to voltage (0..5 V).
LA mode:  6-bit digital channels (PB0:PB5) plotted as stacked traces.

Usage:
    python3 tools/scope.py /dev/ttyUSB0              # live windowed scope
    python3 tools/scope.py /dev/ttyUSB0 --ascii     # plain text render
    python3 tools/scope.py capture.bin               # offline replay (window)
    python3 tools/scope.py capture.bin --png out.png# render one frame to PNG
    python3 tools/scope.py /dev/ttyUSB0 --roll 4096 --fps 15
    python3 tools/scope.py /dev/ttyUSB0 --web   # render in a browser tab

OSC scales: samples are 8-bit MSB-aligned ADC counts; V = count * 5.0 / 255.
Sample rate: 76.92 kS/s (prescaler 16) => 256 samples ~= 3.33 ms.

LA scales: 6-bit packed byte (PB0:PB5). Values 0..63. x-axis = sample index.
"""
import argparse
import os
import sys
import time

OSC_PAYLOAD = 256
LA_PAYLOAD  = 512
HEADER = b"\xAA\x55"
FOOTER = b"\x0D\x0A"
ADC_VREF = 5.0
OSC_SPS = 76_923.08        # 256 samples @ prescaler-16 ADC


def extract_frames(raw, preferred_payload=None):
    """Extract valid AA 55 ... 0D 0A frames from raw bytes.

    Returns (frames_list, unconsumed_remainder) where frames_list contains
    (payload_len, body) tuples.
    """
    i = 0
    n = len(raw)
    frames = []
    payload_sizes = (LA_PAYLOAD, OSC_PAYLOAD)
    if preferred_payload == OSC_PAYLOAD:
        payload_sizes = (OSC_PAYLOAD, LA_PAYLOAD)
    elif preferred_payload == LA_PAYLOAD:
        payload_sizes = (LA_PAYLOAD, OSC_PAYLOAD)

    last_consumed = 0
    while i < n - OSC_PAYLOAD - 3:
        if raw[i] == 0xAA and raw[i + 1] == 0x55:
            matched = False
            for L in payload_sizes:
                end = i + L + 2
                if end + 2 <= n and raw[end : end + 2] == FOOTER:
                    body = raw[i + 2 : i + 2 + L]
                    if len(body) == L:
                        frames.append((L, body))
                        i = end + 2
                        last_consumed = i
                        matched = True
                        break
            if matched:
                continue
        i += 1
    return frames, raw[last_consumed:]


def parse_frames(raw, preferred_payload=None):
    """Yield (payload_len, body) for every valid AA 55 ... 0D 0A frame."""
    frames, _ = extract_frames(raw, preferred_payload)
    yield from frames


def ascii_view(payload_len, samples):
    if payload_len == LA_PAYLOAD:
        channels = []
        for ch in range(6):
            bits = ["#" if (s >> ch) & 1 else "." for s in samples]
            channels.append("".join(bits[:80]))     # truncate to 80 cols
        summary = "LA {} samples | PB0..PB5 channels (sample 0..80):".format(payload_len)
        return summary + "\n" + "\n".join(
            "  PB{}: {}".format(ch, channels[ch]) for ch in range(6)
        )
    lo = min(samples)
    hi = max(samples)
    bars = ["#" if v >= 128 else "." for v in samples]
    pk = hi - lo
    avg = sum(samples) / len(samples)
    return (
        f"min={lo:3d}  max={hi:3d}  pk2pk={pk:3d}  avg={avg:5.1f}  "
        f"({lo*ADC_VREF/255:4.2f}..{hi*ADC_VREF/255:4.2f} V)\n"
        + "".join(bars)
    )


def open_source(device):
    if device in ("/dev/ttyUSB0", "/dev/ttyACM0", "-") or "/tty" in device:
        try:
            import serial
        except ImportError:
            sys.exit("live serial needs pyserial (add to shell.nix / pip install)")
        ser = serial.Serial(device, 1000000, timeout=0.05)
        return "live", ser.read, device
    with open(device, "rb") as fh:
        data = fh.read()
    idx = [0]

    def reader(n):
        chunk = data[idx[0] : idx[0] + n]
        idx[0] += n
        return chunk

    return "file", reader, device


def run_ascii(source, reader, name):
    print(f"{'live: locked on' if source == 'live' else 'replaying'} {name}")
    stash = b""
    sep = 0
    while True:
        chunk = reader(4096)
        if not chunk:
            if source == "live":
                continue
            break
        raw = stash + chunk
        for plen, fr in parse_frames(raw):
            print(ascii_view(plen, fr))
            sep += 1
            if sep >= 8:
                print("...")
                sep = 0
        stash = raw[-520:] if len(raw) >= 520 else raw


def run_plot(source, reader, name, roll, fps, web=False):
    import matplotlib

    if web:
        matplotlib.use("WebAgg")
        matplotlib.rcParams["webagg.port"] = _free_port(8988)
        matplotlib.rcParams["webagg.port_retries"] = 0
        matplotlib.rcParams["webagg.open_in_browser"] = True
        matplotlib.rcParams["webagg.address"] = "127.0.0.1"
    else:
        matplotlib.use(os.environ.get("MPLBACKEND", "TkAgg"))
    import matplotlib.pyplot as plt
    import numpy as np
    import webbrowser

    fig, ax = plt.subplots(figsize=(11, 5))
    fig.canvas.manager.set_window_title(f"ATmega328P MSO - {name}")
    (line,) = ax.plot([], [], lw=1.2, color="#1f77b4")
    ax.set_ylim(0, 5.0)
    ax.set_ylabel("Voltage [V]")
    ax.set_xlabel("time [ms]")
    ax.grid(True, alpha=0.3)
    ax.set_title("waiting for frames ...")
    fig.canvas.draw()

    tape = np.zeros(0, dtype=np.float32)
    kind = None             # "osc" or "la", fixed for the session
    n_frame = 0
    stash = b""
    last = 0.0
    period = 1.0 / fps
    plt.show(block=False)
    if web:
        url = f"http://127.0.0.1:{matplotlib.rcParams['webagg.port']}/"
        print(f"[scope] open in your browser: {url}")
        try:
            webbrowser.open(url)
        except Exception:
            pass
    try:
        while True:
            chunk = reader(4096)
            if not chunk:
                if source == "live":
                    time.sleep(0.01)
                    continue
                break
            frames, stash = extract_frames(raw)
            for plen, fr in frames:
                fk = "la" if plen == LA_PAYLOAD else "osc"
                if kind is None:
                    kind = fk
                    if kind == "la":
                        ax.set_ylabel("digital state (packed PB0:PB5)")
                        ax.set_xlabel("sample index")
                        ax.set_ylim(-0.5, 63.5)
                    else:
                        ax.set_ylim(-0.08, 5.08)
                        ax.set_xlabel("time [ms]")
                elif kind != fk:
                    continue
                n_frame += 1
                if kind == "la":
                    arr = np.frombuffer(fr, dtype=np.uint8).astype(np.float32)
                else:
                    arr = np.frombuffer(fr, dtype=np.uint8).astype(np.float32) \
                        * (ADC_VREF / 255.0)
                tape = arr
            if not kind or not tape.size:
                continue
            if source == "live" and time.monotonic() - last < period:
                continue
            last = time.monotonic()
            if kind == "la":
                xs = np.arange(len(tape))
                ylbl = f"sample={len(tape)}"
            else:
                xs = np.arange(len(tape)) / OSC_SPS * 1000.0
                ylbl = f"min={tape.min():.2f}V  max={tape.max():.2f}V"
            line.set_data(xs, tape)
            ax.relim()
            ax.autoscale_view(scalex=True, scaley=False)
            ax.set_title(f"{name} [{kind.upper()}]  frame#{n_frame}  "
                         f"samples={len(tape)}  {ylbl}")
            fig.canvas.draw()
            fig.canvas.flush_events()
    except KeyboardInterrupt:
        pass
    if source == "file":
        print(f"rendered {n_frame} frame(s); close the window to exit")
        plt.show(block=True)


def render_png(reader, out_path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    stash = b""
    last_plen = 0
    last = b""
    counts = 0
    while True:
        chunk = reader(4096)
        if not chunk:
            break
        raw = stash + chunk
        stash = raw[-520:] if len(raw) >= 520 else raw
        for plen, fr in parse_frames(raw):
            last_plen = plen
            last = fr
            counts += 1
    if counts == 0:
        sys.exit(f"no valid frame (AA55...0D0A) found in file")
    samples = np.frombuffer(last, dtype=np.uint8).astype(float)
    kind = "LA" if last_plen == LA_PAYLOAD else "OSC"
    if kind == "OSC":
        samples *= ADC_VREF / 255.0
    fig, ax = plt.subplots(figsize=(11, 4))
    if kind == "LA":
        x = np.arange(len(samples))
        ax.step(x, samples, color="#1f77b4")
        ax.set_ylim(-0.5, 63.5)
        ax.set_xlabel("sample index")
    else:
        x = np.arange(len(samples)) / OSC_SPS * 1000.0
        ax.plot(x, samples, color="#1f77b4")
        ax.set_ylim(0, 5)
        ax.set_xlabel("time [ms]")
        ax.set_ylabel("Voltage [V]")
    ax.grid(True, alpha=0.3)
    ax.set_title(f"{kind} frame {counts} of {out_path}")
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"wrote {out_path} ({counts} {kind} frame(s))")


def have_module(name):
    try:
        from importlib import import_module
        import_module(name)
        return True
    except Exception:
        return False


def _free_port(desired):
    """Return `desired` if free, else any free ephemeral port (127.0.0.1)."""
    import socket
    for candidate in (desired, 0):
        s = socket.socket()
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("127.0.0.1", candidate))
            port = s.getsockname()[1]
            return port
        except OSError:
            pass
        finally:
            s.close()
    return 8988


def main(argv=None):
    ap = argparse.ArgumentParser(description="AVR MSO host viewer")
    ap.add_argument("device", help="serial device (/dev/ttyUSB0) or capture file")
    ap.add_argument("--ascii", action="store_true", help="text render instead of window")
    ap.add_argument("--png", help="save one frame to this PNG and exit")
    ap.add_argument("--roll", type=int, default=2048, help="samples in the window (default 2048)")
    ap.add_argument("--fps", type=int, default=15, help="redraw rate (default 15)")
    ap.add_argument("--web", action="store_true",
                    help="render in a browser tab (WebAgg) instead of a native window")
    args = ap.parse_args(argv)

    try:
        from importlib import import_module
        import_module("matplotlib")
        have_plot = True
    except Exception:
        have_plot = False

    source, reader, name = open_source(args.device)
    if args.png:
        render_png(reader, args.png)
        return 0

    if args.ascii or not have_plot:
        if not args.ascii and not have_plot:
            print("[scope] matplotlib not importable; using ASCII mode.")
            print("[scope] hint: run inside 'nix-shell shell.nix' (or:",
                  "python3 -m pip install matplotlib numpy pyserial)")
        run_ascii(source, reader, name)
        return 0

    try:
        run_plot(source, reader, name, args.roll, args.fps, web=args.web)
    except Exception as exc:
        if not args.web:
            if not have_module("tornado"):
                print(f"[scope] native window failed ({exc}); browser fallback "
                      "needs tornado.")
                print("[scope] hint: run inside the nix-shell (tornado ships "
                      "there) e.g. nix-shell shell.nix --run ...; or for your "
                      "system python: python3 -m pip install tornado")
                run_ascii(source, reader, name)
                return 0
            print(f"[scope] native window failed ({exc}); "
                  "retrying in browser (WebAgg)...")
            try:
                run_plot(source, reader, name, args.roll, args.fps, web=True)
                return 0
            except Exception as exc2:
                print(f"[scope] browser backend failed ({exc2}); using ASCII mode")
        else:
            if not have_module("tornado"):
                print("[scope] browser backend needs tornado.")
                print("[scope] hint: python3 -m pip install tornado "
                      "(or run inside the nix-shell, which ships it).")
            else:
                print(f"[scope] browser backend failed ({exc}); using ASCII mode")
        run_ascii(source, reader, name)
    return 0


if __name__ == "__main__":
    sys.exit(main())