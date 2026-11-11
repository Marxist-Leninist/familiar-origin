/* BIOS hands off in protected mode; explicitly enter x86-64 long mode. */
.section .text.entry,"ax"
.code32
.global _entry
.extern kernel_main
.extern __bss_start
.extern __bss_end
_entry:
    cli
    cld
    movl $0x90000,%esp
    movl $0x80000000,%eax
    cpuid
    cmpl $0x80000001,%eax
    jb unsupported
    movl $0x80000001,%eax
    cpuid
    testl $0x20000000,%edx
    jz unsupported
    /* Three identity-mapping tables at 0x1000, 0x2000 and 0x3000. */
    movl $0x1000,%edi
    xorl %eax,%eax
    movl $3072,%ecx
    rep stosl
    movl $0x2003,0x1000
    movl $0x3003,0x2000
    movl $0x3000,%edi
    movl $0x83,%eax
    movl $512,%ecx
1:  movl %eax,(%edi)
    addl $0x200000,%eax
    addl $8,%edi
    loop 1b
    movl %cr4,%eax
    orl $0x620,%eax /* PAE, OSFXSR, OSXMMEXCPT. */
    movl %eax,%cr4
    movl $0x1000,%eax
    movl %eax,%cr3
    movl $0xc0000080,%ecx
    rdmsr
    orl $0x100,%eax
    wrmsr
    lgdt gdt_pointer
    movl %cr0,%eax
    andl $0xfffffff3,%eax
    orl $0x80000002,%eax
    movl %eax,%cr0
    ljmp $8,$long_entry
unsupported:
    hlt
    jmp unsupported
.code64
long_entry:
    movw $16,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss
    movq $0x90000,%rsp
    xorq %rbp,%rbp
    fninit
    ldmxcsr initial_mxcsr(%rip)
    movq $__bss_start,%rdi
    movq $__bss_end,%rcx
    subq %rdi,%rcx
    xorl %eax,%eax
    rep stosb
    call kernel_main
2:  cli
    hlt
    jmp 2b
.align 8
gdt:
    .quad 0
    .quad 0x00209a0000000000
    .quad 0x0000920000000000
gdt_end:
gdt_pointer:
    .word gdt_end-gdt-1
    .long gdt
initial_mxcsr:
    .long 0x1f80
.section .note.GNU-stack,"",@progbits
