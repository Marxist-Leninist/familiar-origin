ENTRY(_entry)
PHDRS {
    code PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    stack PT_GNU_STACK FLAGS(6);
}
SECTIONS {
    . = 0x10000;
    .text : { *(.text.entry) *(.text*) } :code
    .rodata : { *(.rodata*) } :code
    . = ALIGN(4096);
    .data : { *(.data*) } :data
    . = ALIGN(16);
    __bss_start = .;
    .bss : { *(.bss*) *(COMMON) } :data
    __bss_end = .;
    ASSERT(__bss_end < 0x80000, "Kernel overlaps reserved stack space")
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
