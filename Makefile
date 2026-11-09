# GFAM build (needs clang + ld.lld + llvm-objcopy + python3)
VERSION := $(shell cat VERSION)
$(shell mkdir -p build)
ROM     = build/GFAM-v$(VERSION).gba
CC      = clang --target=armv4t-none-eabi -mcpu=arm7tdmi
COMMON  = -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-unwind-tables \
          -fno-asynchronous-unwind-tables -O2 -Wall -Wextra -Wno-unused-parameter -Isrc -DGFAM_VERSION='"$(VERSION)"' $(EXTRA)
THUMB   = $(COMMON) -mthumb
ARM     = $(COMMON) -marm

OBJ = build/crt0.o build/main.o build/ui.o build/params.o build/runtime.o build/synth.o build/audio.o build/fx.o

all: $(ROM)

src/gfx.h src/tables.h src/tiles.h: tools/gen_assets.py tools/font.py
	python3 tools/gen_assets.py

build/crt0.o: src/crt0.s
	$(CC) -c $< -o $@
build/synth.o: src/synth.c src/synth.h src/tables.h src/fx.h
	$(CC) $(ARM) -c $< -o $@
build/fx.o: src/fx.c src/fx.h src/synth.h src/tables.h
	$(CC) $(ARM) -c $< -o $@
build/audio.o: src/audio.c src/audio.h src/synth.h
	$(CC) $(ARM) -c $< -o $@
build/main.o: src/main.c src/gfx.h src/synth.h src/ui.h src/audio.h
	$(CC) $(THUMB) -c $< -o $@
build/%.o: src/%.c src/synth.h src/tiles.h VERSION
	$(CC) $(THUMB) -Os -c $< -o $@      # control code (UI, params): size matters more than speed

build/gfam.elf: $(OBJ) src/link.ld
	ld.lld -T src/link.ld --gc-sections -o $@ $(OBJ)
build/gfam.raw: build/gfam.elf
	llvm-objcopy -O binary $< $@
# The ROM is built WITHOUT the Nintendo logo field (see NOTICE.md). For your own device, fill it from a
# ROM you own:  make LOGO_FROM=/path/to/a-rom-you-own.gba   (do not publish that build)
$(ROM): build/gfam.raw VERSION tools/fix_header.py
	python3 tools/fix_header.py $< $@ $(VERSION) $(if $(LOGO_FROM),--logo-from $(LOGO_FROM))

clean:
	rm -f build/*.o build/*.elf build/*.raw build/*.gba
.PHONY: all clean
