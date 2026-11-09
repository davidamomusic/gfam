# Releases

The ROM of each version, `GFAM-v<VERSION>.gba` (64 KB).

| Version | File | SHA-256 |
| --- | --- | --- |
| 6.5 | [`GFAM-v6.5.gba`](GFAM-v6.5.gba) | `822698ec9b397163abe8bb502c852aa0fd67ca6c10d9bad43b33d43465f01d8f` |

Check a download with `sha256sum -c SHA256SUMS` (in this folder).

## Reproducible build

The file is what `make` builds from the source of the same version, byte for byte. It was built with clang and ld.lld 18.1.3
(Ubuntu), `llvm-objcopy` and Python 3.13:

```sh
git checkout <the commit of the version>
make
sha256sum build/GFAM-v6.5.gba
```

Another toolchain version can produce a different file with the same behaviour. `sh tools/run_tests.sh` also checks that every ROM in this folder has the logo field empty.

## What the header has

Title `GFAM`, game code `ZGFM`, the version byte and the header checksum. **The logo field (`0x04`–`0x9F`) is empty**: it is where the
Nintendo logo goes, the logo is Nintendo's, and this repository contains no Nintendo material (see [NOTICE.md](../NOTICE.md)).

So the ROM runs in emulators that use their own built-in BIOS. A real BIOS (real hardware, flash carts, or an emulator that loads a BIOS
file and plays its start-up) may refuse it. For your own device you can build a copy with the logo field filled from a ROM you own
(`make LOGO_FROM=file.gba`); do not publish that copy.

## To add a version

1. Change `VERSION`, build, and run `sh tools/run_tests.sh`.
2. Copy the ROM here and add its line to `SHA256SUMS` and to the table above.
3. Add the entry to [`CHANGELOG.md`](../CHANGELOG.md) and run `python3 tools/make_screens.py` and `python3 tools/make_cheatsheet.py`.
