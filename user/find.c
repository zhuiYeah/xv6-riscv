//在当前目录或在指定路径中查找特定文件名的文件

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//获取 文件名
//比如说 ，从“path = "usr/bin/loacl/gdb” 中获得 文件名“gdb”
char *fmtname(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;

    //查找最后一个 / 之后的第一个字符，由p指向该字符
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name. 返回空白填充的名称。
    if (strlen(p) >= DIRSIZ)
        return p;
    //将p指向的字符串完全复制一份到buf上 , 即将“gdb”复制到buf之上
    memmove(buf, p, strlen(p));
    //将‘ ’复制到字符串 buf + strlen(p) 的前 14 - strlen(p)的字符中 ；
    //即现在buf是“gdb”，有效位置只有前三位，将之后的位全部变为 0(代表无效位)
    // memset(buf + strlen(p), 0, DIRSIZ - strlen(p));

    //或者只将字符串紧临着的后一位置标为0 
    buf[strlen(p)] = 0;
    return buf;
}

void find(char *path, char *fileName)
{
    char buf[512], *p;
    // fd是文件描述符
    int fd;
    //结构体 dirent 存储目录的信息
    struct dirent de;
    //结构体 stat 存储文件的信息
    struct stat st;

    //调用open系统调用打开文件流 并获得文件描述符
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // fstat系统调用 用于从文件描述符fd中获取文件状态，写入st之中
    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    //如果是path是文件和设备的话，比较该文件/设备 的名字是否与fileName相等
    case T_DEVICE:
    case T_FILE:
        if (strcmp(fmtname(path), fileName) == 0)
        {
            printf("%s\n", path);
        }
        break;

        //如果path是目录的话 ,find(de.name,fileName)
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        // 复制 path（目录） 字符串 到buf中
        strcpy(buf, path);
        // p指向buf字符串的尾巴，并再加上 “/” ，于是 buf变为 “目录名path/”
        p = buf + strlen(buf);
        *p++ = '/';
        //从fd中读取到目录信息，相当于遍历目录中的所有文件名de.name
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            //将 目录中的当前文件名称de.name 复制到 到p地址处的前14个字符 ，于是buf变为“目录名path/文件名”
            //即在buf的尾巴处填上
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            find(buf, fileName);
        }
        break;
        close(fd);
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        char *p = ".";
        find(p, argv[1]);
    }
    else if (argc == 3)
    {
        find(argv[1], argv[2]);
    }
    else
    {
        fprintf(2, "正确用法 : find 路径 目标文件名\n");
        fprintf(2, "正确用法2 : find 目标文件名\n");
        exit(1);
    }
    exit(0);
}