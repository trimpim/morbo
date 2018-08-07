OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)
EXTERN(_start)

PHDRS {
      all PT_LOAD ;
      reserve PT_LOAD FLAGS(7);
}

SECTIONS
{
  .cpu_ap AP_CODE :
  {
  . += 4K;
  } : reserve

  /* 1.0 MB */
  . = 0x100000;
  _image_start = .;

  .text :
  {
    *(.text._start)
    *(.text .text.*);
    *(.rodata .rodata.*);
  } : all

  .data :
  {
    *(.data .data.*);
  }

  .bss :
  {
    *(COMMON);
    *(.bss  .bss.*);
    . = ALIGN(0x80000);
    _image_end = . - 1;
  }

  /DISCARD/ :
  {
    *(.not.GNU-stack)
    *(.comment)
  }
}
