/*实现系统调用函数 System call tracing
在这个任务中，user/trace.c 文件已经给出，重点是内核态部分
你将创建一个新的 trace 系统调用来控制跟踪。
它应该有一个参数，一个整数“mask(掩码)”，其指定要跟踪的系统调用。例如，为了跟踪 fork 系统调用，程序调用 trace (1 << SYS_fork) ，其中 SYS_fork 是来自 kernel/syscall.h 的系统调用号。
例如trace 32 grep hello README
如果掩码中设置了系统调用的编号，则必须修改 xv6 内核以在每个系统调用即将返回时打印出一行。
该行应包含 进程 ID 、 系统调用名称 和 返回值 ；您不需要打印系统调用参数。 trace 系统调用应该为调用它的进程和它随后派生的任何子进程启用跟踪，但不应影响其他进程。
*/

/*
第一步，Makefile
根据实验指导书，首先需要将 $U/_trace 添加到 Makefile 的 UPROGS 字段中。
*/

/*
第二步，添加声明
可以看到，在 user/trace.c 文件中，使用了 trace 函数，因此需要在 user/user.h 文件中加入函数声明：int trace(int);
同时，为了生成进入中断的汇编文件，需要在 user/usys.pl 添加进入内核态的入口函数的声明：entry("trace");，以便使用 ecall 中断指令进入内核态；
同时在 kernel/syscall.h 中添加系统调用的指令码SYS_trace ，这样就可以编译成功了。
*/

/*
第三步，在 kernel/sysproc.c 中添加 sys_trace ()函数
实现 sys_trace() 函数，就是在当前trace进程的proc结构中设置好mask，告诉trace进程你需要跟踪的系统调用有哪些
最主要的部分是实现 sys_trace() 函数，在 kernel/sysproc.c 文件中。目的是实现内核态的 trace() 函数。
我们的目的是跟踪程序调用了哪些系统调用函数，因此需要在每个 trace 进程中，添加一个 mask 字段，用来识别是否执行了 mask 标记的系统调用。
在执行 trace 进程时，如果进程调用了 mask 所包括的系统调用，就打印到标准输出中。

首先在 kernel/proc.h 文件中，为 proc 结构体添加 mask 字段：int mask;。
然后在 sys_trace() 函数中，为该字段进行赋值，赋值的 mask 为系统调用传过来的参数，
放在了 a0 寄存器中。使用 argint() 函数可以从对应的寄存器中取出参数并转成 int 类型。
*/

/*
第四步，跟踪子进程
修改 fork ()(参见 kernel/proc.c) ，将trace mask从父进程复制到子进程。
需要跟踪所有 trace 进程下的子进程，在 kernel/proc.c 的 fork() 代码中，设置子进程的 mask与父进程相同；
*/

/*
第五步，打印信息
所有的系统调用都需要通过 kernel/syscall.c 中的 syscall() 函数来执行，因此在这里添加判断
修改 kernel/syscall.c 中的 syscall ()函数以打印跟踪输出。为了获得系统调用的名称，需要添加要索引到的系统调用名称数组。
*/


#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//trace mask  command ..
//例如 trace 32 grep hello README 
//read的系统调用id是5 ， 而 1<<5 = 32 ;所以本条命令的意思是跟踪grep hello RADME 进行过程中的所有read系统调用
//如果 mask 是 33 ；fork的系统调用id是1 ， 于是跟踪在命令执行过程中的 所有 read 、fork系统调用
int main(int argc, char *argv[])
{
    int i;
    char *nargv[MAXARG];
    //argv[1]是mask，mask必须是数字
    if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9'))
    {
        fprintf(2, "Usage: %s mask command ...\n", argv[0]);
        exit(1);
    }
    //mask转换失败
    if (trace(atoi(argv[1])) < 0)
    {
        fprintf(2, "%s: trace failed\n", argv[0]);
        exit(1);
    }

    //至此，已经成功将proc.h中的 trace进程状态proc 的mask设置成功，已经确定了trace进程执行要跟踪的系统调用有哪些

    //argv[2]是grep，将要执行的命令以及参数全部写入 nargv
    for (i = 2; i < argc && i < MAXARG; i++)
    {
        nargv[i - 2] = argv[i];
    }
    //执行要跟踪的子进程 ， 由于当前在trace进程中直接执行grep，于是grep夺舍了trace，占用了trace的pid
    exec(nargv[0], nargv);

    //printf("trace成功结束并退出"); //这句话未执行，为什么？
    exit(1);
}
