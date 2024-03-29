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

int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

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
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    // alarm
    // if there's a process running and if the timer interrupt came from user space
    if(myproc() != 0 && (tf->cs & 3) == 3) {
        // if alarm is set
        if(myproc()->alarmticks != 0 && myproc()->alarmhandler != 0) {
            myproc()->alarm_ticks_passed++;
            if(myproc()->alarm_ticks_passed % myproc()->alarmticks == 0) {
                // push eip to stack to make the process return to user program after executing alarm handler
                tf->esp -= 4;
                *((uint *)(tf->esp)) = tf->eip; // this eip is controlled by user program, so it's not safe, and we need to check if it's valid before using it as the return address
                // set eip in trap frame to alarm handler to run it after the eip is popped from stack
                // directly calling alarm handler will let the user program run with kernel privilege
                tf->eip = (uint)myproc()->alarmhandler;
                // this is not enough though
                // we also need to save other registers and restore them after alarm handler returns
                // however, we now rely on the fact that alarm handler will return to the return address we set before, which is the eip of the user program
                // so one solution, which is the trampoline page in xv6, is to set the return address to a specific program that does the clean job and then return to the user program
                // there may be other solutions
            }
        }
    }
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
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

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }

    // laze page allocation
    if(tf->trapno == T_PGFLT){
        uint addr = rcr2();
        struct proc *curproc = myproc();
        uint a = PGROUNDDOWN(addr);
        char *mem;
        mem = kalloc();
        if(mem == 0){
            cprintf("allocuvm out of memory\n");
            break;
        }
        memset(mem, 0, PGSIZE);
        if(mappages(curproc->pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
            cprintf("allocuvm out of memory (2)\n");
            kfree(mem);
            break;
        }
        break;
    }

    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
