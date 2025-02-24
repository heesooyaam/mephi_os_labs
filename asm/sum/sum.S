        .global sum
        .global sum_struct
        .global sum_array
        .global sum_args

// input:
//    rdi: первое int64_t число
//    rsi: второе int64_t число
//
// output:
//    rax: сумма чисел
//
sum:
    movq %rdi, %rax
    addq %rsi, %rax
    ret

// input:
//    rdi: указатель на структуру
//
// struct {
//     int64_t a;
//     int64_t b;
//     int64_t sum;
// };
//
// output:
//    в поле sum нужно записать сумму a и b
//
sum_struct:
    movq (%rdi), %rax
    addq 8(%rdi), %rax
    movq %rax, 16(%rdi)
    ret

// rcx: индекс в массиве
// rax: текущая сумма
add_element:
    addq (%rdi, %rcx, 8), %rax
    incq %rcx

for_loop:
    cmpq %rcx, %rsi
    jg add_element
    ret


// input:
//    rdi: указатель на массив int64_t
//    rsi: количество элементов в массиве
//
// output:
//    rax: сумма элементов массива
//
sum_array:
    xorq %rax, %rax
    xorq %rcx, %rcx
    call for_loop
    ret

// input:
//    rdi: количество элементов на стеке (сразу после адреса возврата из функции)
//
// output:
//    rax: сумма элементов со стека
//

add_element2:
    addq (%rsp, %rcx, 8), %rax
    incq %rcx

for_loop2:
    cmpq %rcx, %rdi
    jg add_element2
    ret

sum_args:
    movq $2, %rcx
    addq $2, %rdi
    xorq %rax, %rax
    call for_loop2
    ret
