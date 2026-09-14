{ pkgs ? import <nixpkgs> {} }:

let
  avr = pkgs.pkgsCross.avr;
  avrBuild = avr.buildPackages;
  avrlibcOut = avr.avrlibc.outPath;   # avr-libc (cross, host=avr), already in avr-gcc's closure
in
pkgs.mkShell {
  name = "avr-mso";

  packages = [
    avrBuild.gcc            # avr-gcc cross compiler (bundles avrlibc in its closure)
    avrBuild.binutils       # avr-ar, avr-objcopy, avr-size, avr-ld
    avrBuild.gnumake        # GNU make
    pkgs.avrdude            # flash tool for Arduino Nano bootloader
    pkgs.simavr             # AVR simulator used by 'make test'
    (pkgs.python3.withPackages (ps: [
      ps.pyserial           # live serial viewing (tools/scope.py)
      ps.matplotlib         # live windowed scope plot
      ps.numpy              # rolling sample buffer math
      ps.tornado            # matplotlib WebAgg (browser) backend
    ]))
  ];

  # Expose the avr-libc include/lib directories for direct use.
  NIX_AVRLIBC = avrlibcOut;

  shellHook = ''
    echo "AVR development shell ready"
    echo "  avr-gcc : $(command -v avr-gcc)"
    echo "  avrlibc : ${avrlibcOut} (via avr-gcc closure)"
    echo "  make    : $(command -v make)"
    echo "  python3 : $(command -v python3)"
    echo "  pyserial: $(python3 -c 'import serial, sys; print(serial.__version__)' 2>/dev/null || echo missing)"
  '';
}