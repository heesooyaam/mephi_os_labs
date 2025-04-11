#include <inc/types.h>
#include <kern/sysgate.h>
#include <kern/syscall.h>

long read_msr(int msr) {
    int low, high;
    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((long) high << 32) | low;
}

void write_msr(int msr, long value) {
    int low = value & 0xFFFFFFFF;
    int high = value >> 32;
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}

extern void _start_user(void);
void sysgate() {
    static bool already_init = false;
    if (already_init) {
        int syscall_num;
        __asm__ volatile(
            "movl %%edi, %0\n\t"
            : "=r"(syscall_num)
        );

        asm volatile(
            "push %%rcx\n\t"
            "push %%r11\n\t"
            : : : "rcx", "r11"
        );

        long ret_val = 5;
        if (syscall_num == 1) {
            void *arg;
            __asm__ volatile("movq %%rsi, %0" : "=r"(arg));
            sys_work(arg);
        } else if (syscall_num == 2) {
            sys_retire();
        } else {
            ret_val = -1;
        }

        __asm__ volatile("movq %0, %%rax" : : "r"(ret_val));
        __asm__ volatile(
            "pop %%r11\n\t"
            "pop %%rcx\n\t"
            : : : "rcx", "r11"
        );
        __asm__ volatile("sysretq\n\t");
    } else {
        already_init = true;
        write_msr(IA32_EFER, read_msr(IA32_EFER) | 1);

        long user_rflags;
        __asm__ volatile("pushfq; pop %0" : "=r"(user_rflags));

        write_msr(IA32_LSTAR, (long) sysgate);

        __asm__ volatile(
            "movq %0, %%rcx\n\t"
            "movq %1, %%r11\n\t"
            "sysretq\n\t"
            :
            : "r"(_start_user), "r"(user_rflags)
            : "rcx", "r11"
        );
    }
}
