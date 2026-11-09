ENTRY(_start)

MEMORY
{
  rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
  iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 0x7E00   /* top 512 B kept for stacks */
  ewram (rw)  : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS
{
  .text : {
    KEEP(*(.header))
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > rom

  .iwram : ALIGN(4) {
    __iwram_start = .;
    *(.iwram.text .iwram.text.*)
    *(.iwram.data .iwram.data.*)
    *(.data .data.*)
    . = ALIGN(4);
    __iwram_end = .;
  } > iwram AT> rom
  __iwram_load = LOADADDR(.iwram);

  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > iwram

  .ewram (NOLOAD) : ALIGN(4) { *(.ewram .ewram.*) } > ewram

  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) *(.ARM.attributes) }
}
