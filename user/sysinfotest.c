//来自lab2 syscall，用于测试sysinfotest系统调用

/*
第一步，添加 Makefile
根据实验指导书，首先需要将 $U/_sysinfotest 添加到 Makefile 的 UPROGS 字段中
*/

/*
第二步，添加声明
同样需要添加一些声明才能进行编译，启动 qemu。需要以下几步：
1.在 user/user.h 文件中加入函数声明：int sysinfo(struct sysinfo*);，同时添加结构体声明 struct sysinfo;；
2.在 user/usys.pl 添加进入内核态的入口函数的声明：entry("sysinfo");；
3.同时在 kernel/syscall.h 中添加系统调用的指令码。
*/

/*
第三步，获取内存信息
读懂 kernel/kalloc.c 文件，在其中创建free_mem（）以获得空闲内存
*/

/*
第四步，获取进程数目
读懂kernel/proc.c文件 ， 在其中创建 n_proc() 获得进程总数
*/

/*
第五步，声明和调用
在 kernel/defs.h 中添加 free_mem()和 n_proc()的声明
然后在 kernel/sysproc.c 中 创建 sys_sysinfo() 并 进行调用：
*/
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

void sinfo(struct sysinfo *info)
{
    //注意到sysinfo()的参数为指向sysinfo结构体的指针，这也是a0寄存器保存的内容
    if (sysinfo(info) < 0)
    {
        printf("FAIL: sysinfo failed");
        exit(1);
    }
}


//使用 sbrk() 计算有多少空闲物理内存页。
int countfree()
{
    uint64 sz0 = (uint64)sbrk(0);
    struct sysinfo info;
    int n = 0;

    while (1)
    {
        if ((uint64)sbrk(PGSIZE) == 0xffffffffffffffff)
            break;
        n += PGSIZE;
    }
    sinfo(&info);
    if (info.freemem != 0)
    {
        printf("FAIL: there is no free mem, but sysinfo.freemem=%d\n",
               info.freemem);
        exit(1);
    }
    sbrk(-((uint64)sbrk(0) - sz0));
    return n;
}

void testcall()
{
    struct sysinfo info;

    if (sysinfo(&info) < 0)
    {
        printf("FAIL: sysinfo failed\n");
        exit(1);
    }

    if (sysinfo((struct sysinfo *)0xeaeb0b5b00002f5e) != 0xffffffffffffffff)
    {
        printf("FAIL: sysinfo succeeded with bad argument\n");
        exit(1);
    }
}

void testmem()
{
    struct sysinfo info;
    uint64 n = countfree();

    sinfo(&info);

    if (info.freemem != n)
    {
        printf("FAIL: free mem %d (bytes) instead of %d\n", info.freemem, n);
        exit(1);
    }

    if ((uint64)sbrk(PGSIZE) == 0xffffffffffffffff)
    {
        printf("sbrk failed");
        exit(1);
    }

    sinfo(&info);

    if (info.freemem != n - PGSIZE)
    {
        printf("FAIL: free mem %d (bytes) instead of %d\n", n - PGSIZE, info.freemem);
        exit(1);
    }

    if ((uint64)sbrk(-PGSIZE) == 0xffffffffffffffff)
    {
        printf("sbrk failed");
        exit(1);
    }

    sinfo(&info);

    if (info.freemem != n)
    {
        printf("FAIL: free mem %d (bytes) instead of %d\n", n, info.freemem);
        exit(1);
    }
}


void testproc()
{
    struct sysinfo info;
    uint64 nproc;
    int status;
    int pid;

    sinfo(&info);
    nproc = info.nproc;

    pid = fork();
    if (pid < 0)
    {
        printf("sysinfotest: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        sinfo(&info);
        if (info.nproc != nproc + 1)
        {
            printf("sysinfotest: FAIL nproc is %d instead of %d\n", info.nproc, nproc + 1);
            exit(1);
        }
        exit(0);
    }
    wait(&status);
    sinfo(&info);
    if (info.nproc != nproc)
    {
        printf("sysinfotest: FAIL nproc is %d instead of %d\n", info.nproc, nproc);
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    printf("sysinfotest: start\n");
    testcall();
    testmem();
    testproc();
    printf("sysinfotest: OK\n");
    exit(0);
}
