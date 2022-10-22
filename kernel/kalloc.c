// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // 内核之后的第一个地址，defined by kernel.ld.

// run就是空闲链表的一个节点，每个节点存放一个 空闲物理内存页的指针
struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist; //物理内存分配的空闲链表
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

//释放pa指向的物理内存页。注意，pa指针通常应该是通过调用kalloc()返回的（初始化内存allocator时例外，参见kinit（））
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.先给这4096B要释放的物理内存填满垃圾
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  //将pa作为空闲链表的head，更新空闲链表
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

//分配一个4KB（4096B）的物理内存页。返回一个内核可以使用的指针。如果无法分配内存则返回0。
void *kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  //如果r不为null，即r！=0，表明分配成功
  if (r)
    kmem.freelist = r->next; //更新空闲链表
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}


//获得空闲内存总量，单位为B。为了满足sysinfo系统调用所创建
uint64 free_mem(void)
{
  struct run *r = kmem.freelist;
  uint64 pageNum = 0;
  while (r != 0)
  {
    pageNum++;
    r = r->next;
  }
  return pageNum * PGSIZE;
}