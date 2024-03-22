#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  backtrace();  //调用函数打印函数栈帧

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/*
extern uint64 sys_sigalarme(void);
extern uint64 sys_sigalarm(void);

*/


// 调用这个函数，也就是说某个用户进程需要进行一个定时，
// 那么在proc结构体中设置一个变量来记录当前已经用的时间。
// 在调用该函数的时候，进行相应值的初始化;
// 
uint64
sys_sigalarm(void)
{
  //对interval,p_func进行赋值;?参数怎么获取
  int interval;
  uint64 handler;
  struct proc * p;
  if(argint(0, &interval) < 0 || argaddr(1, &handler) < 0 || interval < 0) {
    return -1;
  }
  p = myproc();
  p->interval = interval;
  p->p_func = (void*)handler;
  p->past_time = 0;
  return 0;
}


uint64
sys_sigreturn(void)
{
  struct proc *p=myproc();
  memmove(p->trapframe, p->alarm_trapframe, sizeof(struct trapframe));
  p->is_alarming = 0;
  return 0;
}
