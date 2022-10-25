#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC]; //该数组保存了所有进程

struct proc *initproc; //用户初始进程

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);

static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
//为每个进程的内核栈分配一个页（为每个进程分配一个内核栈）。将其映射到内存中的高位，后面是一个无效的保护页。
void proc_mapstacks(pagetable_t kpgtbl)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        char *pa = kalloc();
        if (pa == 0)
            panic("kalloc");
        uint64 va = KSTACK((int)(p - proc));
        kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
}

// initialize the proc table.
void procinit(void)
{
    struct proc *p;

    initlock(&pid_lock, "nextpid");
    initlock(&wait_lock, "wait_lock");
    for (p = proc; p < &proc[NPROC]; p++)
    {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        // //这里是 每个进程 内核栈的初始化 ,自lab 3.2之后，不在这里初始化进程的内核栈，而在allocproc()上分配进程内核页表和进程内核栈
        // p->kstack = KSTACK((int)(p - proc));
    }
}

// 必须在禁用中断的情况下调用
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
    int id = r_tp();
    return id;
}

// Return this CPU's cpu struct.
// 必须禁用中断
struct cpu *mycpu(void)
{
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}

// 返回当前的 struct proc * （进程状态），如果没有则返回零。
struct proc *myproc(void)
{
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

int allocpid()
{
    int pid;

    acquire(&pid_lock);
    pid = nextpid;
    nextpid = nextpid + 1;
    release(&pid_lock);

    return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
//在进程表中查找 UNUSED proc。如果找到，将其初始化为 在内核中运行所要求的状态 ，并将该proc以有锁的状态返回
static struct proc *allocproc(void)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->state == UNUSED)
        {
            goto found;
        }
        else
        {
            release(&p->lock);
        }
    }
    return 0;

found:
    p->pid = allocpid();
    p->state = USED;

    // Allocate a trapframe page.
    if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
    {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // An empty user page table.
    p->pagetable = proc_pagetable(p);
    if (p->pagetable == 0)
    {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    //这段代码为 lab3 的A kernel page table per process 实验所创建，初始化 用户进程 的内核态页表
    p->kpagetable = ukvminit();
    if (p->kpagetable == 0)
    {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    //为lab3 的A kernel page table per process 实验所创建，
    //目的是 初始化内核栈 并 让进程的内核页表 能映射到 该进程的 内核栈
    //代码移植自 procinit()
    void *pa = kalloc();
    if (pa == 0)
        panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    p->kstack = va;
    ukvmmap(p->kpagetable, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;

    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
//释放进程的 struct proc以及其中的所有数据。调用该函数时p必须持有锁
static void freeproc(struct proc *p)
{
    //来自lab3.2。 释放内核栈 内存 。不释放 进程的内核页表吗? 释放，还没写呢别急
    if (p->kstack)
    {
        pte_t *pte = walk(p->kpagetable, p->kstack, 0);
        if (pte == 0)
            panic("freeproc: walk");
        kfree((void *)PTE2PA(*pte));
    }
    p->kstack = 0;

    //释放进程的内核页表,来自lab3.2
    if (p->kpagetable)
    {
        proc_freewalk(p->kpagetable);
    }
    p->kpagetable = 0;

    if (p->trapframe)
        kfree((void *)p->trapframe);
    p->trapframe = 0;

    if (p->pagetable)
        //释放p进程的全部内存 ， 并释放它的用户页表
        proc_freepagetable(p->pagetable, p->sz);

    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
    p->mask = 0;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
//为给定进程创建用户页表，但不分配用户内存（但是有trampoline 和 trapframe  pages）
pagetable_t proc_pagetable(struct proc *p)
{
    pagetable_t pagetable;

    // An empty page table.
    pagetable = uvmcreate();
    if (pagetable == 0)
        return 0;

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    if (mappages(pagetable, TRAMPOLINE, PGSIZE,
                 (uint64)trampoline, PTE_R | PTE_X) < 0)
    {
        uvmfree(pagetable, 0);
        return 0;
    }

    // map the trapframe page just below the trampoline page, for
    // trampoline.S.
    if (mappages(pagetable, TRAPFRAME, PGSIZE,
                 (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
    {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
    }

    return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
//释放进程的 用户页表，并释放页表引用的物理内存（即进程全部内存）。
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// 设置第一个用户进程
void userinit(void)
{
    struct proc *p;

    p = allocproc();
    initproc = p;

    //分配一个用户页，并将initcode的指令和数据复制到其中
    uvmfirst(p->pagetable, initcode, sizeof(initcode));
    p->sz = PGSIZE;

    // //将 第一个进程的用户页表的mapping 复制到进程内核页表中
    // pte_t *pte, *kernelPte;
    // pte = walk(p->pagetable, 0, 0);
    // kernelPte = walk(p->kpagetable, 0, 1);
    // *kernelPte = (*pte) & ~PTE_U;

    // prepare for the very first "return" from kernel to user.
    p->trapframe->epc = 0;     // user program counter
    p->trapframe->sp = PGSIZE; // user stack pointer

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    p->state = RUNNABLE;

    release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
    uint64 sz;
    struct proc *p = myproc();

    sz = p->sz;
    if (n > 0)
    {
        if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
        {
            return -1;
        }
    }
    else if (n < 0)
    {
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
    int i, pid;
    struct proc *np;
    struct proc *p = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0)
    {
        return -1;
    }

    // Copy user memory from parent to child.
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
    {
        freeproc(np);
        release(&np->lock);
        return -1;
    }

    // //将进程用户页表的mapping,复制一份到进程内核页表中
    // pte_t *pte , *kernelPte;
    // for (uint64 j = 0; j < p->sz; j += PGSIZE)
    // {
    //     pte = walk(np->pagetable,j,0);
    //     kernelPte = walk(np->kpagetable,j,1);
    //     *kernelPte = (*pte) & ~PTE_U;
    // }

    //设置子进程大小与父进程相同
    np->sz = p->sz;

    //设置子进程的掩码，父进程要跟踪哪些系统调用，子进程也要跟踪一样的
    np->mask = p->mask;

    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);

    // Cause fork to return 0 in the child. a0这里又变成保存函数返回值的寄存器了
    np->trapframe->a0 = 0;

    // increment reference counts on open file descriptors.
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);

    safestrcpy(np->name, p->name, sizeof(p->name));

    pid = np->pid;

    release(&np->lock);

    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

    acquire(&np->lock);
    np->state = RUNNABLE;
    release(&np->lock);

    return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
    struct proc *pp;

    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
        if (pp->parent == p)
        {
            pp->parent = initproc;
            wakeup(initproc);
        }
    }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
    struct proc *p = myproc();

    if (p == initproc)
        panic("init exiting");

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++)
    {
        if (p->ofile[fd])
        {
            struct file *f = p->ofile[fd];
            fileclose(f);
            p->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;

    acquire(&wait_lock);

    // Give any children to init.
    reparent(p);

    // Parent might be sleeping in wait().
    wakeup(p->parent);

    acquire(&p->lock);

    p->xstate = status;
    p->state = ZOMBIE;

    release(&wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
    struct proc *pp;
    int havekids, pid;
    struct proc *p = myproc();

    acquire(&wait_lock);

    for (;;)
    {
        // Scan through table looking for exited children.
        havekids = 0;
        for (pp = proc; pp < &proc[NPROC]; pp++)
        {
            if (pp->parent == p)
            {
                // make sure the child isn't still in exit() or swtch().
                acquire(&pp->lock);

                havekids = 1;
                if (pp->state == ZOMBIE)
                {
                    // Found one.
                    pid = pp->pid;
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                             sizeof(pp->xstate)) < 0)
                    {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    freeproc(pp);
                    release(&pp->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&pp->lock);
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || killed(p))
        {
            release(&wait_lock);
            return -1;
        }

        // Wait for a child to exit.
        sleep(p, &wait_lock); // DOC: wait-sleep
    }
}

/*
这是每个cpu都有一个的调度程序
每个cpu在初始化之后立刻调用自己的调度程序
调度程序永远不会return，它循环做这些事情
  - 选择要运行的进程
  - swtch开始运行该进程
  - 该进程通过swtch将cpu控制权交还给调度程序
*/
void scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();

    c->proc = 0;
    for (;;)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        int found = 0; // qemu模拟器太占cpu，所以改了调度程序
        for (p = proc; p < &proc[NPROC]; p++)
        {
            acquire(&p->lock);
            if (p->state == RUNNABLE)
            {
                // Switch to chosen process.  It is the process's job
                // to release its lock and then reacquire it
                // before jumping back to us.
                p->state = RUNNING;
                c->proc = p;

                //来自lab3.2 修改Scheder () ：将进程的内核页表加载到core的satp 寄存器中。（进程调度时切换内核页,保证进程p执行的时候用的是进程内核页表）
                w_satp(MAKE_SATP(p->kpagetable));
                sfence_vma();
                swtch(&c->context, &p->context);
                kvminithart(); //并在调度后切换回来，修改satp为全局内核页表

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                c->proc = 0;
                found = 1;
            }
            release(&p->lock);
        }

        // qemu模拟器太占cpu，所以改了调度程序
        if (found == 0)
        {
            intr_on();
            asm volatile("wfi");
        }
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
    int intena;
    struct proc *p = myproc();

    if (!holding(&p->lock))
        panic("sched p->lock");
    if (mycpu()->noff != 1)
        panic("sched locks");
    if (p->state == RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    struct proc *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    sched();
    release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
    static int first = 1;

    // Still holding p->lock from scheduler.
    release(&myproc()->lock);

    if (first)
    {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        fsinit(ROOTDEV);
    }

    usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    acquire(&p->lock); // DOC: sleeplock1
    release(lk);

    // Go to sleep.
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    release(&p->lock);
    acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        if (p != myproc())
        {
            acquire(&p->lock);
            if (p->state == SLEEPING && p->chan == chan)
            {
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->pid == pid)
        {
            p->killed = 1;
            if (p->state == SLEEPING)
            {
                // Wake process from sleep().
                p->state = RUNNABLE;
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return -1;
}

void setkilled(struct proc *p)
{
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

int killed(struct proc *p)
{
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
    struct proc *p = myproc();
    if (user_dst)
    {
        return copyout(p->pagetable, dst, src, len);
    }
    else
    {
        memmove((char *)dst, src, len);
        return 0;
    }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
    struct proc *p = myproc();
    if (user_src)
    {
        return copyin(p->pagetable, dst, src, len);
    }
    else
    {
        memmove(dst, (char *)src, len);
        return 0;
    }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
    static char *states[] = {
        [UNUSED] "unused",
        [USED] "used",
        [SLEEPING] "sleep ",
        [RUNNABLE] "runble",
        [RUNNING] "run   ",
        [ZOMBIE] "zombie"};
    struct proc *p;
    char *state;

    printf("\n");
    for (p = proc; p < &proc[NPROC]; p++)
    {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        printf("%d %s %s", p->pid, state, p->name);
        printf("\n");
    }
}

//获得进程总数。为满足sysinfo系统调用所创建
int n_proc(void)
{
    int res = 0;
    struct proc *p;
    // for (p = proc; p != 0; p++)
    for (p = proc; p < &proc[NPROC]; p++)
    {
        if (p->state != UNUSED)
            res++;
    }
    return res;
}