#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



/*使用 pipe() 和 fork() 实现父进程发送一个字符，
子进程成功接收该字符后打印 received ping，再向父进程发送一个字符
，父进程成功接收后打印 received pong。
*/


#define READEND 0
#define WRITEEND 1

int main(int argc, char *argv[])
{
    //建立两个管道,管道是单工通信的
    // parentfd 父进程写子进程读
    // childfd  子进程写父进程读
    int parentfd[2];
    int childfd[2];

    char buf[10];

    pipe(parentfd);
    pipe(childfd);

    int pid = fork();
    if (pid < 0)
    {
        fprintf(2, "fork error");
        exit(1);
    }
    //子进程
    //子进程从parentfd读取，并向childfd写入
    else if (pid == 0)
    {
        close(parentfd[WRITEEND]);
        close(childfd[READEND]);
        read(parentfd[READEND], buf, 4);
        printf("我是子进程  %d : received %s \n", getpid(), buf);
        write(childfd[WRITEEND], "pong", 4);
        close(childfd[WRITEEND]);
    }
    //父进程
    //父进程向parentfd写入 ，并从childfd读取
    else
    {
        close(parentfd[READEND]);
        close(childfd[WRITEEND]);
        write(parentfd[WRITEEND], "ping", 4);
        close(parentfd[WRITEEND]);
        read(childfd[READEND], buf, 4);
        printf("我是父进程 %d : received %s \n ", getpid, buf);
    }

    exit(0);
}