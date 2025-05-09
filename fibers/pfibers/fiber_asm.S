    .text
    .globl SwitchFiberContext
    .type  SwitchFiberContext, @function

/*
struct Context {
    size_t r8, r9, r10, r11, r12, r13, r14, r15;
    size_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp;
    size_t rip, flags;
};
*/

// void SwitchFiberContext(*from - rdi, *to - rsi)
SwitchFiberContext:
    // сохраняем текущие состояния регистров
    pushfq
    popq 136(%rdi)

    movq %r8, 0(%rdi)
    movq %r9, 8(%rdi)
    movq %r10, 16(%rdi)
    movq %r11, 24(%rdi)
    movq %r12, 32(%rdi)
    movq %r13, 40(%rdi)
    movq %r14, 48(%rdi)
    movq %r15, 56(%rdi)
    movq %rdi, 64(%rdi)
    movq %rsi, 72(%rdi)
    movq %rbp, 80(%rdi)
    movq %rbx, 88(%rdi)
    movq %rdx, 96(%rdi)
    movq %rax, 104(%rdi)
    movq %rcx, 112(%rdi)
    movq %rsp, 120(%rdi)

    // загружаем нужные состояния регистров
    movq 0(%rsi), %r8
    movq 8(%rsi), %r9
    movq 16(%rsi), %r10
    movq 24(%rsi), %r11
    movq 32(%rsi), %r12
    movq 40(%rsi), %r13
    movq 48(%rsi), %r14
    movq 56(%rsi), %r15
    movq 64(%rsi), %rdi
    movq 80(%rsi), %rbp
    movq 88(%rsi), %rbx
    movq 96(%rsi), %rdx
    movq 104(%rsi), %rax
    movq 112(%rsi), %rcx
    movq 120(%rsi), %rsp

    pushq 136(%rsi)
    popfq

    movq 72(%rsi), %rsi

    ret
