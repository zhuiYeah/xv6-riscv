#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    int i, count = 0, k, m = 0;
    // lineSplit数组将储存xrags后面的所有的执行参数,一共有count个执行参数，count标记 下一个执行参数 应该填在lineSplit中的位置，
    char *lineSplit[MAXARG], *p;
    // block不断从标准输出读取 ， buf中会不断存放并更新执行参数（\n分割执行参数），m标记 从标准输出读取的字符 应该填在buf的哪一个位置
    char block[32], buf[32];
    // p指向buf中的执行参数
    p = buf;
    for (i = 1; i < argc; i++)
    {
        lineSplit[count++] = argv[i];
    }

    //从标准输出读取内容到block之中，k是实际读取的长度
    while ((k = read(0, block, sizeof(block))) > 0)
    {
        for (i = 0; i < k; i++)
        {
            if (block[i] == '\n')
            {
                buf[m] = 0;
                //将前一个执行参数（由p指向），填入执行参数列表lineSplit
                lineSplit[count] = p;
                count++;
                // lineSplit终止于此
                lineSplit[count] = 0;
                // m、p、count回到初始位置
                m = 0;
                p = buf;
                count = argc - 1;
                if (fork() == 0)
                {
                    // argv[0]是xrags ， argv[1]是echo（例）
                    exec(argv[1], lineSplit);
                }
                wait(0);
            }
            else if (block[i] == ' ')
            {
                //当前位置分开了两个执行参数
                buf[m] = 0;
                m++;
                lineSplit[count++] = p;
                p = &buf[m];
            }
            else
            {
                buf[m] = block[i];
                m++;
            }
        }
    }
    exit(0);
}