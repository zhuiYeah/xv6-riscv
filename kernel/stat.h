#define T_DIR     1   // 目录
#define T_FILE    2   // 文件
#define T_DEVICE  3   // 设备

struct stat {
  int dev;     // 文件系统的磁盘设备
  uint ino;    // 文件索引号
  short type;  // 文件类型 1，2，3
  short nlink; // Number of links to file  
  uint64 size; // 文件大小（以字节为单位）
};
