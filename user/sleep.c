#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//对于bash中 sleep 10 这样的合法输入，argc的长度一定为2
// argv[0]指针一定指向sleep ， argv[1]指针一定指向 10
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(2, "正确用法:sleep <number>");
        //fprintf(2, "usage: sleep <number>\n");
        exit(1);
    }
    int number = atoi(argv[1]);
    sleep(number);
    exit(0);
}