/*
sysproc.c 文件 ,与进程有关的系统调用
从这里向寄存器取得 真正实现系统调用需要的参数，之后有可能需要转入proc.c 进行更复杂更细节的处理
*/

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"


uint64 sys_exit(void)
{
  int n;
  //从a0寄存器中读取其内容并转化为int形式写入n中
  argint(0, &n);
  exit(n);
  // not reached
  return 0;
}

uint64 sys_getpid(void)
{
  return myproc()->pid;
}

//这里会跳转到proc.c中的fork(),它是真正的进行创建进程、进程内存复制的函数
uint64 sys_fork(void)
{
  return fork();
}

uint64 sys_wait(void)
{
  uint64 p;
  //从a0寄存器中取得地址
  argaddr(0, &p);
  return wait(p);
}

uint64 sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64 sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_trace(void)
{
  //掩码，指示trace需要跟踪哪些系统调用
  int mask;
  // a0寄存器保存着trace的第一个参数，也即mask
  argint(0, &mask);
  if (mask < 0)
    return -1;
  myproc()->mask = mask;
  return 0;
}
