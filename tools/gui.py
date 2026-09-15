#!/usr/bin/env python3
"""gui.py - ATmega328P MSO control panel + live viewer.

Talks the ASCII opcode protocol implemented in Controller_Program.c:
    'R'/'r'  RUN          (only honored in STOPPED state)
    'S'/'s'  STOP         (ForceStop: halts everything)
    'O'/'o'  Oscilloscope mode   (only honored in STOPPED state)
    'L'/'l'  Logic Analyzer mode (only honored in STOPPED state)
    '0'..'7' Time/Div scale      (MSO_TIMEDIV_10US..50MS, enum index)

Streams received:
    OSC frames: AA 55 + 256 samples + 0D 0A   (260 bytes, 8-bit ADC -> V)
    LA  frames: AA 55 + 512 samples + 0D 0A   (516 bytes, packed PB0:PB5)

Usage:
    python3 tools/gui.py                       # /dev/ttyUSB0 @ 1 Mbps
    python3 tools/gui.py /dev/ttyACM0 --baud 250000
    python3 tools/gui.py --file capture.bin    # offline replay (no serial)

Requires: pyserial, matplotlib, numpy (ships in shell.nix).
"""
import argparse
import os
import sys
import threading
import time

try:
    from scope import parse_frames, extract_frames, OSC_PAYLOAD, LA_PAYLOAD, FOOTER
except ImportError:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from scope import parse_frames, extract_frames, OSC_PAYLOAD, LA_PAYLOAD, FOOTER

import tkinter as tk
from tkinter import ttk

import numpy as np
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

ADC_VREF = 5.0
OSC_FREE_RUN_DT_US = 13.0          # effective dt when DSO clamps to free-run

# dt [us] per timediv index, mirroring Controller_Program.c timer configs
OSC_DT_US = {
    0: OSC_FREE_RUN_DT_US,                         # 10 us  (free-run ~13 us)
    1: OSC_FREE_RUN_DT_US,                         # 50 us  (clamped fast ~13 us)
    2: OSC_FREE_RUN_DT_US,                         # 100 us (clamped fast ~13 us)
    3: OSC_FREE_RUN_DT_US,                         # 500 us (free-run ~13 us)
    4: (78 + 1) * 0.5,                              # 1 ms   (OCR1A=78, /8 => 39.5 us)
    5: (390 + 1) * 0.5,                             # 5 ms   (OCR1A=390, /8 => 195.5 us)
    6: (780 + 1) * 0.5,                             # 10 ms  (OCR1A=780, /8 => 390.5 us)
    7: (3906 + 1) * 0.5,                            # 50 ms  (OCR1A=3906, /8 => 1953.5 us)
}
LA_DT_US = {
    0: (1 + 1) * 0.0625,                            # 10 us  (OCR1A=1, /1 => 0.125 us)
    1: (4 + 1) * 0.0625,                            # 50 us  (OCR1A=4, /1 => 0.3125 us)
    2: (8 + 1) * 0.0625,                            # 100 us (OCR1A=8, /1 => 0.5625 us)
    3: (39 + 1) * 0.0625,                           # 500 us (OCR1A=39, /1 => 2.5 us)
    4: (78 + 1) * 0.0625,                           # 1 ms   (OCR1A=78, /1 => 4.9375 us)
    5: (390 + 1) * 0.0625,                          # 5 ms   (OCR1A=390, /1 => 24.4375 us)
    6: (780 + 1) * 0.0625,                          # 10 ms  (OCR1A=780, /1 => 48.8125 us)
    7: (3906 + 1) * 0.0625,                         # 50 ms  (OCR1A=3906, /1 => 244.1875 us)
}
TIMEDIV_LABELS = ["10 us", "50 us", "100 us", "500 us",
                  "1 ms", "5 ms", "10 ms", "50 ms"]

ROLL_SAMPLES = 2048          # scrolling window for OSC (>= 256)
LA_ROLL = 1536               # scrolling window for LA (= 3 frames)

# ---- dark theme palette ----
C_BG       = "#1e1e2e"
C_PANEL    = "#2a2a3c"
C_PANEL2   = "#313244"
C_TEXT     = "#d8dee9"
C_MUTED    = "#8a93a5"
C_ACCENT   = "#61afef"
C_OK       = "#98c379"
C_ERR      = "#e06c75"
C_WARN     = "#e5c07b"


class MsoGui:
    def __init__(self, root, port, baud, replay=None):
        self.root = root
        self.port = port
        self.baud = baud
        self.replay = replay

        self.ser = None
        self.thread = None
        self.alive = False
        self.buf = bytearray()
        self.lock = threading.Lock()
        self.stash = b""

        self.mode = "osc"            # requested mode
        self.timediv_idx = 4         # default 1 ms, matches MCU boot
        self.running = False
        self.kind = None             # stream-inferred 'osc' | 'la'
        self.tape = np.zeros(0, dtype=np.float32)
        self.frames = 0
        self.bytes_total = 0
        self.t0 = time.monotonic()
        self.t_last = self.t0

        self._build_ui()
        if replay:
            self._load_replay(replay)
        else:
            self.ser = None          # connect happens on button press

        self._after_id = None
        self.root.after(40, self._poll)

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        self.root.title("ATmega328P MSO - controller & scope")
        self.root.configure(bg=C_BG)
        self.root.geometry("1180x680")
        self.root.minsize(900, 520)

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("TFrame", background=C_BG)
        style.configure("Panel.TFrame", background=C_PANEL)
        style.configure("TLabel", background=C_BG, foreground=C_TEXT)
        style.configure("Panel.TLabel", background=C_PANEL, foreground=C_TEXT)
        style.configure("Title.TLabel", background=C_BG, foreground=C_ACCENT,
                        font=("DejaVu Sans", 15, "bold"))
        style.configure("Muted.TLabel", background=C_PANEL, foreground=C_MUTED)

        main = ttk.Frame(self.root)
        main.pack(fill="both", expand=True)
        sidebar = ttk.Frame(main, style="Panel.TFrame", width=248)
        sidebar.pack(side="left", fill="y")
        sidebar.pack_propagate(False)
        body = ttk.Frame(main, style="TFrame")
        body.pack(side="right", fill="both", expand=True)

        # ---------- title ----------
        tk.Label(sidebar, text="ATmega328P MSO", bg=C_PANEL,
                 fg=C_ACCENT, font=("DejaVu Sans", 14, "bold")).pack(
            anchor="w", padx=14, pady=(14, 2))
        tk.Label(sidebar, text="Oscilloscope + Logic Analyzer",
                 bg=C_PANEL, fg=C_MUTED, font=("DejaVu Sans", 9)).pack(
            anchor="w", padx=14, pady=(0, 10))

        # ---------- serial ----------
        self._section(sidebar, "SERIAL LINK")
        row = ttk.Frame(sidebar, style="Panel.TFrame")
        row.pack(fill="x", padx=14, pady=(2, 2))
        ttk.Label(row, text="Port", style="Panel.TLabel").pack(side="left")
        self.port_var = tk.StringVar(value=self.port)
        port_cb = ttk.Combobox(row, textvariable=self.port_var, width=13,
                               values=self._list_ports())
        port_cb.pack(side="right")
        port_cb.bind("<<ComboboxSelected>>", lambda e: None)
        row2 = ttk.Frame(sidebar, style="Panel.TFrame")
        row2.pack(fill="x", padx=14)
        ttk.Label(row2, text="Baud (U2X)", style="Panel.TLabel").pack(side="left")
        self.baud_var = tk.StringVar(value=str(self.baud))
        ttk.Combobox(row2, textvariable=self.baud_var, width=13,
                     values=["500000", "1000000"]).pack(side="right")
        self.con_btn = tk.Button(
            sidebar, text="CONNECT", command=self._toggle_connect,
            bg=C_OK, fg="#0b0d14", activebackground="#a8dea0",
            activeforeground="#0b0d14", relief="flat", font=("DejaVu Sans", 10, "bold"))
        self.con_btn.pack(fill="x", padx=14, pady=6)
        self.link_lbl = tk.Label(sidebar, text="offline", bg=C_PANEL,
                                 fg=C_MUTED, font=("DejaVu Sans", 9))
        self.link_lbl.pack(anchor="w", padx=14)

        # ---------- mode ----------
        self._section(sidebar, "MODE")
        mode_row = ttk.Frame(sidebar, style="Panel.TFrame")
        mode_row.pack(fill="x", padx=14)
        self.mode_var = tk.StringVar(value="Oscilloscope")
        for label, val in (("Oscilloscope", "osc"),
                           ("Logic Analyzer", "la")):
            tk.Radiobutton(mode_row, text=label, value=val,
                           variable=self.mode_var, command=self._apply_mode,
                           bg=C_PANEL, fg=C_TEXT, selectcolor=C_PANEL2,
                           activebackground=C_PANEL, activeforeground=C_TEXT,
                           highlightthickness=0).pack(anchor="w")

        # ---------- time/div ----------
        self._section(sidebar, "TIME / DIV")
        td_row = ttk.Frame(sidebar, style="Panel.TFrame")
        td_row.pack(fill="x", padx=14)
        self.td_var = tk.StringVar(value=TIMEDIV_LABELS[self.timediv_idx])
        td_cb = ttk.Combobox(td_row, textvariable=self.td_var, width=12,
                             values=TIMEDIV_LABELS, state="readonly")
        td_cb.pack(fill="x")
        td_cb.bind("<<ComboboxSelected>>", self._apply_timediv)

        # ---------- acquisition ----------
        self._section(sidebar, "ACQUISITION")
        btn_row = ttk.Frame(sidebar, style="Panel.TFrame")
        btn_row.pack(fill="x", padx=14)
        self.run_btn = tk.Button(
            btn_row, text="RUN", command=self._on_run,
            bg=C_OK, fg="#0b0d14", relief="flat", width=7,
            font=("DejaVu Sans", 11, "bold"), activebackground="#a8dea0")
        self.run_btn.pack(side="left", fill="x", expand=True, padx=(0, 4))
        self.stop_btn = tk.Button(
            btn_row, text="STOP", command=self._on_stop,
            bg=C_ERR, fg="#0b0d14", relief="flat", width=7,
            font=("DejaVu Sans", 11, "bold"), activebackground="#ee8f95")
        self.stop_btn.pack(side="right", fill="x", expand=True, padx=(4, 0))

        trig_row = ttk.Frame(sidebar, style="Panel.TFrame")
        trig_row.pack(fill="x", padx=14, pady=(8, 2))
        self.led = tk.Canvas(trig_row, width=14, height=14,
                             bg=C_PANEL, highlightthickness=0)
        self.led.pack(side="left")
        self.led_id = self.led.create_oval(2, 2, 12, 12, fill=C_MUTED, outline="")
        ttk.Label(trig_row, text="streaming", style="Muted.TLabel",
                  foreground=C_MUTED).pack(side="left", padx=6)
        self.state_lbl = tk.Label(sidebar, text="state: idle", bg=C_PANEL,
                                  fg=C_MUTED, font=("DejaVu Sans", 9))
        self.state_lbl.pack(anchor="w", padx=14)

        # ---------- stats ----------
        self._section(sidebar, "STATUS")
        self.stat_lbl = tk.Label(sidebar, text="frames: 0\nbytes: 0\nrate: -",
                                 bg=C_PANEL, fg=C_TEXT, justify="left",
                                 font=("DejaVu Sans Mono", 9))
        self.stat_lbl.pack(anchor="w", padx=14)

        # ---------- plot ----------
        plt.rcParams.update({
            "figure.facecolor": C_BG, "axes.facecolor": C_PANEL,
            "axes.edgecolor": C_PANEL2, "axes.labelcolor": C_TEXT,
            "xtick.color": C_MUTED, "ytick.color": C_MUTED,
            "grid.color": C_PANEL2, "text.color": C_TEXT,
        })
        self.fig, self.ax = plt.subplots(figsize=(10, 5.5))
        self.fig.subplots_adjust(left=0.07, right=0.97, top=0.93, bottom=0.11)
        (self.line,) = self.ax.plot([], [], lw=1.1, color=C_ACCENT)
        self.ax.grid(True, alpha=0.35)
        self.ax.set_title("open a serial port and press RUN", color=C_MUTED)
        self.ax.set_ylabel("Voltage [V]")
        self.ax.set_xlabel("time [ms]")
        self.canvas = FigureCanvasTkAgg(self.fig, master=body)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        self._set_led(False)

    def _section(self, parent, text):
        tk.Label(parent, text=text, bg=C_PANEL, fg=C_WARN,
                 font=("DejaVu Sans", 9, "bold")).pack(
            anchor="w", padx=14, pady=(12, 2))

    @staticmethod
    def _list_ports():
        try:
            from serial.tools import list_ports
            return [p.device for p in list_ports.comports()]
        except Exception:
            return []

    # ------------------------------------------------------------- serial
    def _toggle_connect(self):
        if self.ser is not None:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        try:
            import serial
            ser = serial.Serial(self.port_var.get(),
                                int(self.baud_var.get()), timeout=0.05)
        except Exception as exc:
            self._flash_link(f"link error: {exc}", C_ERR)
            return
        self.ser = ser
        self.alive = True
        with self.lock:
            self.buf.clear()
        self.stash = b""
        self.kind = "osc" if self.mode == "osc" else "la"
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()
        self.con_btn.configure(text="DISCONNECT", bg=C_ERR)
        self._flash_link("linked @ {} baud".format(self.baud_var.get()), C_OK)
        self.state_lbl.config(text="state: stopped (press RUN)")

    def _disconnect(self):
        self.alive = False
        if self.thread:
            self.thread.join(timeout=1.0)
            self.thread = None
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.con_btn.configure(text="CONNECT", bg=C_OK)
        self._flash_link("offline", C_MUTED)
        self._set_led(False)
        self.running = False

    def _reader(self):
        while self.alive:
            try:
                data = self.ser.read(4096)
            except Exception:
                break
            if not data:
                continue
            with self.lock:
                self.buf += data

    # ------------------------------------------------------------------ cmds
    def _send(self, cmd):
        if self.ser is None:
            return
        try:
            self.ser.write(cmd.encode("ascii"))
            self.ser.flush()
        except Exception:
            self._disconnect()

    def _apply_mode(self):
        """Mode switch is only honored by the MCU while STOPPED."""
        new = self.mode_var.get()
        if new == "osc":
            cmd = "O"
        else:
            cmd = "L"
        if self.ser is not None:
            self._send("S")
            self._send(cmd)
            if self.running:
                self._send("R")
        self.mode = "osc" if new == "osc" else "la"
        self.kind = self.mode
        with self.lock:
            self.buf.clear()
        self.stash = b""
        self.tape = np.zeros(0, dtype=np.float32)
        self._clear_plot()

    def _apply_timediv(self, _evt):
        self.timediv_idx = TIMEDIV_LABELS.index(self.td_var.get())
        if self.ser is not None:
            self._send(str(self.timediv_idx))

    def _on_run(self):
        with self.lock:
            self.buf.clear()
        self.stash = b""
        self.kind = "osc" if self.mode == "osc" else "la"
        self._send("R")
        self.running = True
        self.state_lbl.config(text="state: running (ARMED / stream)")
        self._set_led(True)

    def _on_stop(self):
        self._send("S")
        self.running = False
        self.state_lbl.config(text="state: stopped")
        self._set_led(False)

    # ----------------------------------------------------------- plotting
    def _poll(self):
        if self.ser is not None or self.replay:
            try:
                with self.lock:
                    if self.buf:
                        new = bytes(self.buf)
                        self.buf.clear()
                    else:
                        new = b""
                if new or self.stash:
                    raw = self.stash + new
                    self.bytes_total += len(new)
                    pref = OSC_PAYLOAD if self.mode == "osc" else LA_PAYLOAD
                    frames, self.stash = extract_frames(raw, preferred_payload=pref)
                    for plen, fr in frames:
                        self._consume_frame(plen, fr)
                    if frames:
                        self._update_plot()
                        self._update_stats()
            except Exception:
                pass                    # one bad frame never kills the loop
        try:
            alive = self.root.winfo_exists()
        except tk.TclError:
            return
        if alive:
            self._after_id = self.root.after(40, self._poll)

    def _consume_frame(self, plen, fr):
        fk = "la" if plen == LA_PAYLOAD else "osc"
        expected = "osc" if self.mode == "osc" else "la"
        if fk != expected:
            return                     # mode mismatch: skip stale frames from previous mode
        self.kind = fk
        self.frames += 1
        arr = np.frombuffer(fr, dtype=np.uint8).astype(np.float32)
        if fk == "osc":
            arr = arr * (ADC_VREF / 255.0)
        self.tape = arr

    def _update_plot(self):
        if self.kind is None or not self.tape.size:
            return
        tape = self.tape
        if self.kind == "la":
            n = len(tape)
            x = np.arange(n)
            self.ax.clear()
            self.ax.grid(True, alpha=0.35)
            for ch in range(5, -1, -1):
                bits = (tape.astype(np.uint8) >> ch) & 1
                y = ch + 0.85 * bits
                self.ax.step(x, y, where="post", lw=1.0,
                             color=self._la_color(ch))
            self.ax.set_yticks(range(6))
            self.ax.set_yticklabels([f"PB{c}" for c in range(6)])
            self.ax.set_ylim(-0.5, 5.6)
            dt_us = LA_DT_US[self.timediv_idx]
            self.ax.set_xlabel(f"sample index (~{dt_us:.3f} us/sample)")
            self.ax.set_ylabel("channels (D0=D13..PB0)")
            frames = self.frames
            self.ax.set_title(
                f"LOGIC ANALYZER - {self.mode_str()}  frame #{frames}  "
                f"({n} samples/frame)")
        else:
            dt_us = OSC_DT_US[self.timediv_idx]
            x = np.arange(len(tape)) * dt_us / 1000.0          # ms
            self.ax.clear()
            self.ax.grid(True, alpha=0.35)
            self.ax.plot(x, tape, lw=1.1, color=C_ACCENT)
            self.ax.set_ylim(-0.08, 5.08)
            lo, hi = float(tape.min()), float(tape.max())
            self.ax.set_title(
                f"OSCILLOSCOPE - {self.mode_str()}  frames={self.frames}  "
                f"min={lo:.2f}V  max={hi:.2f}V  "
                f"window={len(x)*dt_us/1000.0:.2f} ms")
            self.ax.set_ylabel("Voltage [V]")
            self.ax.set_xlabel("time [ms]")
        self.canvas.draw_idle()

    @staticmethod
    def _la_color(ch):
        return ["#e5c07b", "#98c379", "#56b6c2", "#c678dd",
                "#e06c75", "#61afef"][ch]

    def mode_str(self):
        return {"osc": "OSC", "la": "LA"}.get(self.kind or self.mode, "?")

    def _clear_plot(self):
        self.ax.clear()
        self.ax.grid(True, alpha=0.35)
        self.ax.set_title("...", color=C_MUTED)
        self.canvas.draw_idle()

    def _update_stats(self):
        now = time.monotonic()
        elapsed = now - self.t_last
        rate = (self.frames / elapsed) if elapsed > 0 else 0.0
        self.t_last = now
        kbps = (self.bytes_total / 1024.0) / max(now - self.t0, 0.001)
        self.stat_lbl.config(
            text="frames: {}\nbytes (last poll): {}\nrate (last poll): {:.0f} f/s\n"
                 "link: {:.0f} kB/s".format(
                self.frames, len(self.tape), rate, kbps))
        if self.frames and self.kind:
            self.state_lbl.config(
                text="state: streaming [{}]".format(self.mode_str()))

    # ------------------------------------------------------------- replay
    def _load_replay(self, path):
        with open(path, "rb") as fh:
            self.buf = bytearray(fh.read())
        self.link_lbl.config(text="replay: {}".format(os.path.basename(path)))
        self.state_lbl.config(text="state: replaying capture")

    # ------------------------------------------------------------- helpers
    def _set_led(self, on):
        color = C_OK if on else C_MUTED
        self.led.itemconfig(self.led_id, fill=color)

    def _flash_link(self, text, color):
        self.link_lbl.config(text=text, fg=color)
        self.root.after(2500,
                        lambda: self.link_lbl.config(fg=C_MUTED))


def main(argv=None):
    ap = argparse.ArgumentParser(description="ATmega328P MSO control GUI")
    ap.add_argument("device", nargs="?", default="/dev/ttyUSB0",
                    help="serial device (default /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=1000000)
    ap.add_argument("--file", help="replay a captured .bin instead of serial")
    args = ap.parse_args(argv)

    root = tk.Tk()
    app = MsoGui(root, args.device or "/dev/ttyUSB0", args.baud,
                 replay=args.file)

    def _on_close():
        app._disconnect()
        if app._after_id is not None:
            try:
                root.after_cancel(app._after_id)
            except tk.TclError:
                pass
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", _on_close)
    try:
        root.mainloop()
    except KeyboardInterrupt:
        _on_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())