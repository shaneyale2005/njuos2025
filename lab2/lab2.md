# 南京大学2025年春季操作系统实验2笔记

本实验需在Linux系统下实现，依赖`/proc`文件系统解析进程树，macOS因缺乏该目录不兼容。

程序需简洁高效，代码控制在100–150行内，通过遍历`/proc/`下的进程目录构建父子关系并递归打印。

## 基础知识

计算机的操作系统给我们提供了“对象+API”，应用程序通过他们组合出各式各样的功能。

日常里我们经常使用的任务管理器等类似的程序其实都是利用操作系统提供给我们的进程信息实现的，例如，`pstree`就是一个可以直观打印父子进程的工具。

`/proc/PID/stat`是Linux内核维护的进程状态文件，包含了一个进程几乎所有的关键信息，它是一个单行、固定格式、高效可解析的二进制文本混合文件。

## 目录操作

使用C语言提供的库`dirent.h`来对目录进行操作，它是一个POSIX标准库头文件，名字来源于directory entry,在代码中使用`#include <dirent.h>`进行操作。

了解`dirent`这个结构体的内部细节。

```c
struct dirent {
    ino_t          d_ino;       // 节点号
    off_t          d_off;       // 偏移量，但是不常用，可以忽略
    unsigned short d_reclen;    // 目录项长度
    unsigned char  d_type;      // 文件类型
    char           d_name[256]; // 文件名（以 '\0' 结尾的字符串）
};
```

核心操作API如下。

- `DIR *opendir(const chat *name)`：打开目录
- `struct dirent *readdir(DIR *dirp)`：读取目录中的下一个目录项，到NULL结束
- `int closedir(DIR *dirp)`：关闭目录








