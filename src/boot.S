/* Familiar Origin: original BIOS stage-one loader, GNU assembler syntax. */
.section .text
.code16
.global _start
_start:
    ljmp $0, $start
start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    sti
    cld
    movb %dl, drive
    /* Select 320x200 indexed-colour graphics mode. */
    movw $0x13, %ax
    int $0x10
    movb drive, %dl
    movw $0x55aa, %bx
    movb $0x41, %ah
    int $0x13
    jc disk_error
    cmpw $0xaa55, %bx
    jne disk_error
    testw $1, %cx
    jz disk_error
    movb drive, %dl
    movw $packet, %si
    movb $0x42, %ah
    int $0x13
    jc disk_error
    cli
    inb $0x92, %al
    orb $2, %al
    andb $0xfe, %al
    outb %al, $0x92
    lgdt gdt_pointer
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmp $8, $protected
disk_error:
    movw $error_text, %si
1:  lodsb
    testb %al, %al
    jz 2f
    movb $0x0e, %ah
    movw $7, %bx
    int $0x10
    jmp 1b
2:  cli
    hlt
    jmp 2b
.balign 4
packet:
    .byte 0x10, 0
    .word 64
    .word 0, 0x1000
    .quad 1
drive: .byte 0
error_text: .asciz "Familiar Origin: BIOS disk read failed."
.balign 8
gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:
gdt_pointer:
    .word gdt_end-gdt-1
    .long gdt
.code32
protected:
    movw $16, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x90000, %esp
    movl $0x10000, %eax
    jmp *%eax
.org 510
.word 0xaa55
