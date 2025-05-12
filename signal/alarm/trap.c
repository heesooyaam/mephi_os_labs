#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  struct proc *p = myproc();

  /* ------------------------------------------------------------------
   * 1.  Закончили пользовательский alarm-handler?
   *     Считаем, что вышли из него тогда и только тогда, когда
   *     - мы снова в user-mode  (cs & 3) == 3
   *     - eip совпал с адресом, куда handler делал RET
   *       (это тот eip, который мы сохранили в alarm_tf->eip)
   * ------------------------------------------------------------------ */
  if (p && p->alarm_in_handler) {
    if ( (tf->cs & 3) == DPL_USER &&
         tf->eip      == p->alarm_tf->eip ) {
      *tf = *p->alarm_tf;          /* вернуть исходные регистры  */
      p->alarm_in_handler = 0;     /* разрешить следующий alarm */
    }
  }

  /* ------------------------------------------------------------------ */
  /* 2.  Системный вызов                                                */
  /* ------------------------------------------------------------------ */
  if (tf->trapno == T_SYSCALL) {
    if (p && p->killed)
      exit();
    p->tf = tf;
    syscall();
    if (p && p->killed)
      exit();
    return;
  }

  /* ------------------------------------------------------------------ */
  /* 3.  Аппаратные и внешние прерывания                                 */
  /* ------------------------------------------------------------------ */
  switch (tf->trapno) {

  /* ——— Таймер ——————————————————————————————— */
  case T_IRQ0 + IRQ_TIMER:
    /* глобальный ticks обновляет только CPU-0 */
    if (cpuid() == 0) {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }

    /* Доставка alarm текущему RUNNING-процессу */
    if (p &&
        p->state == RUNNING &&
        p->alarm_interval > 0 &&
        !p->alarm_in_handler) {

      if (++p->alarm_ticks >= p->alarm_interval) {
        p->alarm_ticks      = 0;
        p->alarm_in_handler = 1;

        /* 1) сохранить полный user-trapframe до сигнала */
        *p->alarm_tf = *tf;

        /* 2) положить return-адрес на пользовательский стек */
        tf->esp -= 4;
        *(uint *)tf->esp = p->alarm_tf->eip;   /* старый eip */

        /* 3) начать выполнять пользовательский handler   */
        tf->eip = (uint)p->alarm_handler;
      }
    }

    lapiceoi();                          /* End-Of-Interrupt */
    break;

  /* ——— IDE ———————————————————————————————— */
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE + 1:             /* возможный ложный interrupt */
    break;

  /* ——— Клавиатура ————————————————————————— */
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;

  /* ——— COM-порт —————————————————————————— */
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;

  /* ——— Spurios ——————————————————————————— */
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  /* ——— Прочие trap-ы ————————————————————— */
  default:
    if (p == 0 || (tf->cs & 3) == 0) {
      /* В kernel-mode — паника ядра */
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    /* В user-mode — отмечаем процесс как убитый */
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x -- kill proc\n",
            p->pid, p->name, tf->trapno, tf->err,
            cpuid(), tf->eip, rcr2());
    p->killed = 1;
  }

  /* ------------------------------------------------------------------ */
  /* 4.  Завершаем процесс, если он убит и мы в user-space               */
  /* ------------------------------------------------------------------ */
  if (p && p->killed && (tf->cs & 3) == DPL_USER)
    exit();

  /* ------------------------------------------------------------------ */
  /* 5.  Добровольная уступка CPU на каждом тикe таймера                 */
  /* ------------------------------------------------------------------ */
  if (p && p->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();

  /* ------------------------------------------------------------------ */
  /* 6.  Повторная проверка killed после yield                           */
  /* ------------------------------------------------------------------ */
  if (p && p->killed && (tf->cs & 3) == DPL_USER)
    exit();
}