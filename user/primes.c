#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
任务是输出区间内的质数 [2, 35]，如果一般的方法，直接挨个遍历计算就行，但由于这里需要充分使用 pipe 和 fork 方法
，因此需要一些不一样的方法。

下面使用的方法是埃拉托斯特尼素数筛，简称筛法。简单地说就是，每次得到一个素数时，
在所有小于 n 的数中，删除它的倍数，然后不断递归，剩下的就全是素数了。

为了用上管道和fork，那么我们就在每一层的递归中分配一些任务给子进程，用子进程将前count个有效数字写进管道
父进程来处理管道中的所有数字，决定改数字的去留，父进程构造了全新的input数组和count


*/

#define READEND 0
#define WRITEEND 1

//给你input数组，以及它里面包含的有效数字总数count，printPrime需要找到其中所有的质数
void printPrime(int *input, int count)
{
    if (count == 0)
    {
        return;
    }
    //第一个数一定是素数，记录这个数字
    int p[2], prime = *input;
    // p管道仅用于在本层递归中父子进程之间的单向通信，子进程写父进程读
    pipe(p);
    //一个buff可以储存一个int类型的数据（需要将int类型转化为（char*）类型）
    char buff[4];
    printf("prime : %d\n", input[0]);

    if (fork() == 0)
    {
        //利用子进程将剩下的所有数全都写到管道中。
        close(p[READEND]);
        for (int i = 0; i < count; i++)
        {
            //将指针（input+i）转化为（char *）写入p管道
            write(p[WRITEEND], (char *)(input+i), 4);
        }
        close(p[WRITEEND]);
    }
    else if (fork() > 0)
    {
        //在父进程中，将数不断读出来，管道中第一个数一定是素数
        //然后删除它的倍数（如果不是它的倍数，就继续更新数组，同时记录数组中目前已有的数字数量）。
        close(p[WRITEEND]);
        count = 0;
        //该指针记录input的初始值
        int * tt = input; 
        //不断从管道中读取一个整数并写入buff中
        while (read(p[READEND], buff, 4) != 0)
        {
            int tmp = *((int *)buff);
            
            //如果从管道中读出来的数字不能被数组中最前列的素数整除的话，那么这个数仍有可能是素数，需要写入数组中
            //而如果可以被整除，它一定不是素数，可以删除他了
            if (tmp % prime != 0)
            {
                count++;
                *input++ = tmp;
            }
        }
        printPrime(tt, count);
        close(p[READEND]);
        //为了在递归中保持同步，父进程需要等待子进程运行结束
        //wait(0);
    }
}

int main(int argc, char *argv[])
{
    // input数组会存放 2 ～ 35 这些数字
    int input[34];
    for (int i = 0; i < 34; i++)
    {
        input[i] = i + 2;
    }
    printPrime(input,34);
    exit(0);
}