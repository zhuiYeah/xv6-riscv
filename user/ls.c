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
  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  // Return blank-padded name. 返回空白填充的名称。
  if (strlen(p) >= DIRSIZ)
    return p;
  //将p指向的字符串完全复制一份到buf上 , 即将“gdb”复制到buf之上
  memmove(buf, p, strlen(p));
  //将‘ ’复制到字符串 buf + strlen(p) 的前 14 - strlen(p)的字符中 ；
  //即现在buf是“gdb”，有效位置只有前三位，将之后的位全部变为‘ ’
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void ls(char *path)
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
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  // fstat系统调用 用于从文件描述符fd中获取文件状态，写入st之中
  if (fstat(fd, &st) < 0)
  {
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type)
  {
    //如果是path是文件和设备的话，打印出它的信息
  case T_DEVICE:
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

    //如果path是目录的话
  case T_DIR:
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
    {
      printf("ls: path too long\n");
      break;
    }
    // buf复制 路径（目录） 字符串
    strcpy(buf, path);
    // p指向buf字符串的尾巴，并再加上 “/” ，于是 buf变为 “目录名/”
    p = buf + strlen(buf);
    *p++ = '/';
    //从fd中读取到目录信息，相当于遍历目录中的所有文件
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
      if (de.inum == 0)
        continue;
      //复制 目录中的某个文件名称 字符串到p地址处的前14个字符 ，于是buf变为“目录名/文件名”
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      //从 文件名称字符串中 获得文件信息写入st之中
      if (stat(buf, &st) < 0)
      {
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[])
{
  int i;

  if (argc < 2)
  {
    ls(".");
    exit(0);
  }
  for (i = 1; i < argc; i++)
    ls(argv[i]);
  exit(0);
}
