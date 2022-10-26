//来自lab4.1 RISC-V assembly,是学习RISC-V指令的一个小问答练习
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x)
{
    return x + 3;
}

int f(int x)
{
    return g(x);
}

void main(void)
{
    /*li	a2,13  将13存入a2寄存器
      a0～a7寄存器储存了函数调用的参数，这里参数13被存在了a2寄存器中
    */
    printf("%d %d\n", f(8) + 1, 13);
    exit(0);
}

/*观看call.asm，main函数中调用函数f的汇编代码在哪？调用函数g的汇编代码在哪？
  答：
      其实不存在这样的调用跳转代码。在call.asm中,g(x)被内联到f(x)中，然后 f(x) 又被进一步内联到 main() 中。
      所以看到的不是函数跳转，而是优化后的内联函数。
*/

/*观看call.asm，printf函数所在的地址是？
   答：
    auipc	ra,0x0 : 将当前程序计数器pc的值 + 0 赋给ra寄存器
    jalr	1552(ra): 
*/



/*观看该代码 unsigned int i = 0x00646c72;
           printf("H%x Wo%s", 57616, &i); 
  输出是什么? 如果 RISC-V 是大端序的，要实现同样的效果，需要将 i 设置为什么？需要将 57616 修改为别的值吗？

 答: 
    %x表示十六进制，57616转换为16进制为 e110;
     %s表示输出字符串 ，以整数 i 所在的开始地址，按照字符的格式读取字符，直到读取到 ‘\0’ 为止。当是小端序表示的时候，内存中存放的数是：72 6c 64 00，刚好对应rld
     于是输出  He110 World ；
     如果是大端序的，57616的十六进制形式就是e110。但对于i来说当是大端序的时候，则反过来了，因此需要将 i 以16进制数的方式逆转一下。
*/


/* 观看该代码，printf("x=%d y=%d", 3);在y= 之后会输出什么？
   prinf（）接收到了两个参数，但他实际需要三个参数，于是 a2寄存器有什么值，他就输出什么
*/