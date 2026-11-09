# Credits

GFAM is the work of one person ([@davidamomusic](https://github.com/davidamomusic)), written with the help of Claude (Anthropic); see
[NOTICE.md](NOTICE.md#authorship).

## Ideas

- The reverb is a small version of the well-known Freeverb design (4 comb filters and 2 all-pass filters).

## Tools

- **clang**, **ld.lld** and **llvm-objcopy** (LLVM): the build of the ROM for the ARM7TDMI.
- **Python 3**: the asset generator, the header tool and the cheat sheet. **Pillow**: the screenshots.
- **gcc**: the host tests.
- **Chrome / Chromium** in headless mode and **pdftoppm** (poppler): the cheat sheet's PDF and its preview images.
- **mGBA**: used to measure the audio load.

## Fonts

- The 5x7 bitmap font of the app is drawn in `tools/font.py` for this project.
- The cheat sheet is set in the system's Helvetica / Arial / Liberation Sans / DejaVu Sans, whichever the PDF was made with. No font file is in the repository.

## Nintendo

Nothing from Nintendo is included in this repository; see [NOTICE.md](NOTICE.md#what-this-repository-contains-and-what-it-does-not).
