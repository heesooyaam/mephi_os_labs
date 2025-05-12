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

  if (p && p->alarm_in_handler) {
    if ( (tf->cs & 3) == DPL_USER &&
         tf->eip != (uint)p->alarm_handler )
    {
      *tf = *p->alarm_tf;
      p->alarm_in_handler = 0;
    }
  }
  if (tf->trapno == T_SYSCALL) {
    if (p && p->killed)
      exit();
    p->tf = tf;
    syscall();
    if (p && p->killed)
      exit();
    return;
  }

  switch (tf->trapno) {

  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0) {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }

    if (p &&
        p->state == RUNNING &&
        p->alarm_interval > 0 &&
        !p->alarm_in_handler) {

      if (++p->alarm_ticks >= p->alarm_interval) {
        p->alarm_ticks = 0;
        p->alarm_in_handler = 1;

        *p->alarm_tf = *tf;

        tf->esp -= 4;
        *(uint*)tf->esp = p->alarm_tf->eip;

        tf->eip = (uint)p->alarm_handler;
      }
    }

    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE + 1:
    break;

  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;

  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  default:
    if (p == 0 || (tf->cs & 3) == 0) {
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x -- kill proc\n",
            p->pid, p->name, tf->trapno, tf->err,
            cpuid(), tf->eip, rcr2());
    p->killed = 1;
  }

  if (p && p->killed && (tf->cs & 3) == DPL_USER)
    exit();

  if (p && p->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();

  if (p && p->killed && (tf->cs & 3) == DPL_USER)
    exit();
}