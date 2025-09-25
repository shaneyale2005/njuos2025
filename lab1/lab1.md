# 南京大学2025年操作系统实验1笔记

## 字符和字符串

在C语言中，字符用单引号包裹，字符串用双引号包裹。例如：

```c
char ch1 = 'a';
char ch2 = '\0';
char ch3[] = "I love you";
```

同时应该熟悉下列常用的字符串相关的操作：
- `strcpy(dest, src)`：将src拷贝到dest
- `strcat(dest, src)`：将src拼接到dest的末尾
- `strcmp(ch1, ch2)`：比较ch1和ch2，如果两个相等就返回0
- `strlen(ch)`：获得一个字符串的长度，不包含`\0`
- `strstr(s, sub)`：查找子串sub在s中第一次出现的位置
- `strchr(s, ch)`：查找字符ch在s中第一次出现的位置

## 文件操作

在C语言中，文件是通过`FILE *`来进行操作的。

### 文件的打开与关闭

使用`fopen(filename, mode)`对文件进行打开。

常用的几种模式：
- r：只读模式
- w：写入，覆盖原文件，默认如果原文件不存在就创建
- a：追加写入，写在文件的末尾
- r+：读写，文件必须存在
- w+：读写，清空原内容
- a+：读写，以追加方式进行读写

使用`fclose(FILE *fp)`对文件进行关闭。

### 文件的读

- `fgets(buffer, size, fp)`：从文件中读一行，写入到buffer中，包括换行符，如果到达了文件的末尾就返回NULL
- `fgetc(fp)`：读取单个字符
- `fread(ptr, size, count, fp)`：按照二进制块来进行读取，用于非文本文件

### 文件的写

- `fprintf(fp, "%s\n", str)`：类似于printf，但是是写入到文件中
- `fputc()`：写入一个字符
- `fwrite(ptr, size, count, fp)`：逐块写，写入到一个二进制块中

## 深度优先搜索

