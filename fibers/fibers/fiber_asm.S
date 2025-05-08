    .text
    .globl SwitchFiberContext
    .type  SwitchFiberContext, @function

// void SwitchFiberContext(*from - rdi, *to - rsi)
SwitchFiberContext:
    // сохраняем текущие состояния регистров (хз, почему не надо сохранять флаги, но у меня работает без этого)
    movq    %rsp, 0(%rdi)
    movq    %rbx, 8(%rdi)
    movq    %rbp, 16(%rdi)
    movq    %r12, 24(%rdi)
    movq    %r13, 32(%rdi)
    movq    %r14, 40(%rdi)
    movq    %r15, 48(%rdi)

    // загружаем нужные состояния регистров
    movq    0(%rsi), %rsp
    movq    8(%rsi), %rbx
    movq    16(%rsi), %rbp
    movq    24(%rsi), %r12
    movq    32(%rsi), %r13
    movq    40(%rsi), %r14
    movq    48(%rsi), %r15

    ret
