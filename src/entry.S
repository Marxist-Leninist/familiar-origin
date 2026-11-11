.section .text.entry,"ax"
.code32
.global _entry
.extern kernel_main
.extern __bss_start
.extern __bss_end
_entry:
    cld
    movl $0x90000, %esp
    xorl %ebp, %ebp
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    xorl %eax, %eax
    rep stosb
    call kernel_main
1:  cli
    hlt
    jmp 1b
.section .note.GNU-stack,"",@progbits
