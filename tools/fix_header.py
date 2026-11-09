#!/usr/bin/env python3
"""Patch the GBA cartridge header (complement check, version) and pad the ROM to a multiple of 64 KB.

usage: fix_header.py in.raw out.gba [version] [--logo-from OTHER.gba]
       (version "2.0" -> header byte 0xBC = 20)

This repository contains NO Nintendo material, so by default the 156-byte logo field of the header
(0x04..0x9F) is left as zeros. The ROM runs in emulators that do not run the real BIOS start-up
(for example mGBA with its built-in BIOS), but the real BIOS (and real hardware) refuses a ROM whose
logo field is empty.

For your own device you can fill that field from a GBA ROM you already own:

    python3 tools/fix_header.py build/GFAM-raw.bin my-copy.gba 6.5 --logo-from /path/to/a-rom-you-own.gba

(with make: `make LOGO_FROM=/path/to/a-rom-you-own.gba`). Do not publish the result: the logo belongs
to Nintendo. See NOTICE.md.
"""
import sys

LOGO_OFFSET, LOGO_LEN = 0x04, 156


def main(argv):
    args, logo_from = [], None
    it = iter(argv)
    for a in it:
        if a == "--logo-from":
            logo_from = next(it, None)
            if not logo_from:
                sys.exit("--logo-from needs a file")
        else:
            args.append(a)
    if len(args) < 2:
        sys.exit(__doc__)
    src, dst = args[0], args[1]
    ver = args[2] if len(args) > 2 else None

    rom = bytearray(open(src, "rb").read())
    rom[LOGO_OFFSET:LOGO_OFFSET + LOGO_LEN] = bytes(LOGO_LEN)        # no logo in the repository's builds
    if logo_from:
        other = open(logo_from, "rb").read(LOGO_OFFSET + LOGO_LEN)
        if len(other) < LOGO_OFFSET + LOGO_LEN or not any(other[LOGO_OFFSET:]):
            sys.exit(f"{logo_from}: no logo data at 0x04..0x9F (is it a GBA ROM?)")
        rom[LOGO_OFFSET:LOGO_OFFSET + LOGO_LEN] = other[LOGO_OFFSET:LOGO_OFFSET + LOGO_LEN]
        print("logo field copied from", logo_from, "(for your own use: do not publish this ROM)")
    if ver:
        a, _, b = ver.partition('.')
        rom[0xBC] = (int(a) * 10 + int(b or 0)) & 0xFF
    chk = 0
    for b in rom[0xA0:0xBD]:
        chk = (chk - b) & 0xFF
    rom[0xBD] = (chk - 0x19) & 0xFF
    pad = (-len(rom)) % 0x10000
    rom += b"\xff" * pad
    open(dst, "wb").write(rom)
    state = "logo field filled (own use only)" if logo_from else "logo field empty"
    print(f"{dst}: {len(rom)} bytes, header check 0x{rom[0xBD]:02X}, {state}")


if __name__ == "__main__":
    main(sys.argv[1:])
