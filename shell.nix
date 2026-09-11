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
    pkgs.simavr             # AVR simulator used by 'make test'
  ];

  # Expose the avr-libc include/lib directories for direct use.
  NIX_AVRLIBC = avrlibcOut;

  shellHook = ''
    echo "AVR development shell ready"
    echo "  avr-gcc : $(command -v avr-gcc)"
    echo "  avrlibc : ${avrlibcOut} (via avr-gcc closure)"
    echo "  make    : $(command -v make)"
  '';
}