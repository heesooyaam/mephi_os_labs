        .global savectx
        .global jumpctx

savectx:
    movq (%rsp), %rsi  // rip
    movq %rsi, (%rdi)

    movq %rsp, 8(%rdi)
    addq $8, 8(%rdi)   // rsp

    movq %rbp, 16(%rdi) // rbp

    xorq %rax, %rax
    ret

jumpctx:
    movq 8(%rdi), %rsp // upd rsp
    movq 16(%rdi), %rbp // upd rbp

    pushq (%rdi) // make ret think that it ends savecntx function

    movq $1, %rax
    ret
