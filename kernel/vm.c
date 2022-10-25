// vm.c存放与页表有关的函数
#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h" //为了lab3.2
#include "proc.h"     //为了lab3.2

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[]; // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
//为内核生成一个 直接映射 页表，由kvminit()调用
pagetable_t kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t)kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // 为每个进程分配和映射一个内核栈。（内核中的每个进程？）
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

//初始化内核页表（唯一的）
void kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
//修改satp为全局内核页表。在swtch返回之后调用
void kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
/*
 The risc-v Sv39 scheme has three levels of page-table
 pages. A page-table page contains 512 64-bit PTEs.
 A 64-bit virtual address is split into five fields:
   39..63 -- must be zero.
   30..38 -- 9 bits of level-2 index.
   21..29 -- 9 bits of level-1 index.
   12..20 -- 9 bits of level-0 index.
   0..11 -- 12 bits of byte offset within the page.
*/
//从虚拟地址va获得对应的页表项PTE 地址。如果alloc！=0，则创建任何需要的页表页。
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--)
  {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V)
    {
      pagetable = (pagetable_t)PTE2PA(*pte);
    }
    else
    {
      if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
//从虚拟地址va映射到物理地址。只能用于查找用户页面。
uint64 walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
//为系统唯一内核页表添加一个页表项。仅在启动时使用。不刷新TLB、不启用分页
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
//虚拟地址从va开始，物理地址从pa开始，需要映射的总长度为size；为va->pa的映射创建PTEs（页表项）。注意va和size可能不是页面对齐的。
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if (size == 0)
    panic("mappages: size");
  //获取与页面对齐的地址a
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for (;;)
  {
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if (*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
//删除页表中从虚拟地址va开始的共npages页映射。va必须是页面对齐的。映射必须存在。可以选择释放对应的物理内存
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if ((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for (a = va; a < va + npages * PGSIZE; a += PGSIZE)
  {
    if ((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if ((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    //释放物理内存
    if (do_free)
    {
      uint64 pa = PTE2PA(*pte);
      kfree((void *)pa);
    }
    //页表项置0，相当于是释放了该页表项
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
//创建一个空的用户页表。如果内存不足，则返回 0。
pagetable_t uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
//将用户初始化代码加载到页表的地址 0 中，用于第一个进程。sz必须小于一页。
void uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if (sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  //分配4096B的物理内存，mem指向该内存区域
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  //建立从 虚拟地址0 -> 物理地址（uint64）的映射
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_X | PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
//使得PTEs和物理内存 将进程的大小从oldsz增长为newsz，不需要页面对齐。
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE)
  {
    mem = kalloc();
    if (mem == 0)
    {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm) != 0)
    {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
//释放用户页面以将进程大小从oldsz变为newsz。oldsz和 newsz不需要是页面对齐的，newsz也不需要小于oldsz。oldsz可以大于实际进程大小。返回新的进程大小。
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
  {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
//// 递归释放页表 占用的物理内存page。所有叶子映射（可读、可写、有效页，即三级页）必须已经被删除。
void freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table
  //一个页表有512个页表项
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
    {
      //该页有效，但不可读不可写不可执行。表明该 PTE 指向一个较低级别（一级或二级）的页表，递归进入它指向的下一层
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      //递归回来之后，再将当前页表项释放
      pagetable[i] = 0;
    }
    else if (pte & PTE_V)
    {
      //该页有效，且可读||可写||可执行(叶子页)，那么认为该页表不能被释放,说明不满足函数的调用条件，你把我页表释放了，那我这个进程就迷路了呀
      panic("freewalk: leaf");
    }
    //该页无效，不用处理
  }
  // 512个页表项指向的内存全部被释放，释放当前页表
  kfree((void *)pagetable);
}

// Free user memory pages,
// then free page-table pages.
//释放用户占据的物理内存pages 共sz字节 ，之后释放 它的页表
void uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
//给定父进程的页表，将其内存复制到子进程的页表中。 不仅复制页表而且复制物理内存(用于fork())。
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE)
  {
    if ((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char *)pa, PGSIZE);
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0)
    {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
//标记一个pte是用户无权限访问的。
void uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
//从内核复制到用户。将内核中的src（共len字节）复制到用户虚拟地址空间的dstva处。在sysinfo系统调用实验中使用。
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while (len > 0)
  {
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
//从用户复制到内核。将 虚拟地址的srcva处 长为len的数据复制到 内核的dst处。
//实现方法是用walkaddr()把用户虚拟指针转化为内核可以直接使用的物理指针,在lab3.3中升级成为了copyin_new()。
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  //这里为了达成长度len需要遍历用户页表。所以lab3.3对他进行升级,直接调用copyin_new();
  while (len > 0)
  {
    //向下对齐
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
  // return copyin_new(pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  // lab3.3对注释部分进行了升级，直接调用copyinstr_new()
  uint64 n, va0, pa0;
  int got_null = 0;

  while (got_null == 0 && max > 0)
  {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    char *p = (char *)(pa0 + (srcva - va0));
    while (n > 0)
    {
      if (*p == '\0')
      {
        *dst = '\0';
        got_null = 1;
        break;
      }
      else
      {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if (got_null)
  {
    return 0;
  }
  else
  {
    return -1;
  }
  // return copyinstr_new(pagetable, dst, srcva, max);
}

//为了lab3 Print a page table  所创建
void _vmprint(pagetable_t pagetable, int level)
{
  if (level == 4)
    return;
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i];
    //如果该页是有效的
    if (pte & PTE_V)
    {
      // pa指向下一级页表
      uint64 pa = PTE2PA(pte);
      for (int j = 0; j < level; j++)
      {
        printf(" .. ");
      }
      printf("%d :    pte %p     pa %p \n", i, pte, pa);
      //如果不可读&&不可写&&不可执行,那么就不是叶子页，继续往下递归
      if ((pte & (PTE_R | PTE_W | PTE_W)) == 0)
      {
        _vmprint((pagetable_t)pa, level + 1);
      }
      // //也可以根据level页表级数来确定要不要递归，如果未到三级都需要往下一级递归。
      // if (level != 3)
      // {
      //   _vmprint((pagetable_t)pa, level + 1);
      // }
    }
  }
}

//为了lab3 Print a page table  所创建
void vmprint(pagetable_t pagetable)
{
  printf("该页表的地址位于 : %p \n", pagetable);
  _vmprint(pagetable, 1);
}

// kvmmap()为系统内核页表添加一个页表项。仅在启动时使用。不刷新TLB、不启用分页,ukvmmap()由该函数改写而来
//为了lab3 A kernel page table per process 创建，用于初始化 per用户进程 独有的内核态页表，给陷入内核态的该进程使用
//为 用户进程内核页表 添加一个页表项
void ukvmmap(pagetable_t kpagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpagetable, va, sz, pa, perm) != 0)
    panic("ukvmmap");
}

//由kvminit() 以及kvmmake()改写而来
//为了lab3 A kernel page table per process 创建，用于初始化 per用户进程 独有的内核态页表，给陷入内核态的该进程使用
pagetable_t ukvminit()
{
  // kvminit();
  // kvmmake();
  pagetable_t kpagetable;

  kpagetable = (pagetable_t)kalloc();
  memset(kpagetable, 0, PGSIZE);

  ukvmmap(kpagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  ukvmmap(kpagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  ukvmmap(kpagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  ukvmmap(kpagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  ukvmmap(kpagetable, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  ukvmmap(kpagetable, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  ukvmmap(kpagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  return kpagetable;
}

//   freewalk();
//释放 进程的内核页表,不释放物理页内存。来自lab3.2 ,参考freewalk();注意freewalk()函数仅释放了页表 ，调用freewalk()要保证所有叶子页已被删除
//而该函数仅释放页表不要求叶子页已被删除，因为进程的内核页表只是进程的用户页表的一个备份，是让进程能够在陷入内核之后仍然摸得着路的手段
void proc_freewalk(pagetable_t kpagetable)
{
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = kpagetable[i];
    if (pte & PTE_V)
    {
      kpagetable[i] = 0;
      if ((pte & (PTE_R | PTE_W | PTE_U)) == 0)
      {
        uint64 child = PTE2PA(pte);
        proc_freewalk((pagetable_t)child);
      }
    }
  }
  kfree((void *)kpagetable);
}

// lab3.3，将进程用户页表复制到进程内核页表。
void u2kvmcopy(pagetable_t upagetable, pagetable_t kpagetable, uint64 oldsz, uint64 newsz)
{
  oldsz = PGROUNDUP(oldsz);
  for (uint64 i = oldsz; i < newsz; i += PGSIZE)
  {
    pte_t *pte_from = walk(upagetable, i, 0);
    pte_t *pte_to = walk(kpagetable, i, 1);
    if (pte_from == 0) panic("u2kvmcopy : src pte 不存在");
    if (pte_to == 0) panic("u2kvmcopy : destpte walk failed ");
    uint64 pa = PTE2PA(*pte_from);
    uint flag = (PTE_FLAGS(*pte_from)) &  (~PTE_U);
    *pte_to = PA2PTE(pa) | flag;
  }
}
