/* 实现思路
1. 遍历/proc获取信息
2. 构建父子关系
3. 打印进程树
4. 命令行参数解析 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h> // 提供对目录进行操作的函数
#include <stdbool.h>

#define MAX_CHILDREN 1024
#define MAX_PROCESSES 32768

// 在这里我们定义一个进程的结构体，这里要理解这个结构体内部有什么
typedef struct Process
{
    int pid; // 进程ID
    int ppid; // 父进程ID
    char name[256]; // 进程的名字
    struct Process *children[MAX_CHILDREN]; // 子进程数组
    int child_count; // 子进程的个数
} Process;

// 这里存储的应该是所有的进程了
Process *processes[MAX_PROCESSES];
Process *all_processes[MAX_PROCESSES];
int proc_count = 0;
// 这里的两个变量用来控制打印的行为
bool show_pids = false;
bool numeric_sort = false;

// 给定一个pid，读取/proc目录获得pid，ppid，name这三个信息
Process *read_process(int pid) {
    char path[256];
    // sprintf()用来把一段格式化的字符串写到内存中的字符数组中
    sprintf(path, "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    Process *p = (Process*)malloc(sizeof(Process));
    p->pid = pid;
    p->child_count = 0;
    /* 这里写的话要理解stat的文件格式
    fscanf()是用来从源文件中读取格式化的数据
    fscanf()要求传入变量的地址，因为它需要往里面写数据
    所以写成&p->pid，取pid这个int成员的地址 */
    fscanf(fp, "%d (%[^)]) %*c %d", &p->pid, p->name, &p->ppid);
    fclose(fp);
    return p;
}

// 遍历/proc
void scan_processes(){
    DIR *dir = opendir("/proc");
    if (!dir) exit(1);
    // dirent代表目录项，也就是子目录或者是目录下文件的信息
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* /proc下的目录名有两类
        第一类是数字目录表示进程PID
        第二类是非数字目录/文件用来表示系统信息文件
        atoi()把字符串转换成整数，如果不是数字就得到0 */
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        Process *p = read_process(pid);
        if (p) {
            processes[pid] = p;
            all_processes[proc_count++] = p;
        }

    }
    closedir(dir);
}

// 递归打印树的辅助函数
int cmp_pid(const Process **a, const Process **b) {
    return (*a)->pid - (*b)->pid;
}

// 递归打印树
void print_tree(Process *p, int level) {
    for (int i = 0; i < level; i++) {
        printf(" ");
    }
    if (show_pids) {
        printf("%s(%d)\n", p->name, p->pid);
    } else {
        printf("%s\n", p->name);
    }

    // 这里在打印子进程之前，必要的时候应该排序
    if (numeric_sort && p->child_count > 1) {
        // 这里的打印排序逻辑值得好好琢磨下，学习stdlib中的qsort()函数
        qsort(p->children, p->child_count, sizeof(Process*), (int(*)(const void*, const void*))cmp_pid);
    }

    // printf("%s\n", p->name);
    for (int i = 0; i < p->child_count; i++) {
        print_tree(p->children[i], level + 1);
    }
}

// 构建父子关系
void build_tree() {
    for (int i = 0; i < proc_count; i++) {
        Process *p = all_processes[i];
        if (p->ppid > 0 && processes[p->ppid]) {
            Process *parent = processes[p->ppid];
            parent->children[parent->child_count++] = p;
        }
    }
}

int main(int argc, char *argv[]) {
    // 这里应该添加对命令行参数的解析部分
    for (int i = 1; i < argc; i++) {
        if (strcmp("-V", argv[1]) == 0 || strcmp("--version", argv[1]) == 0) {
            printf("This is my pstree\n");
            return 0;
        } else if (strcmp("-p", argv[i]) == 0 || strcmp("--show-pids", argv[i]) == 0) {
            show_pids = true;
        } else if (strcmp("-n", argv[i]) == 0 || strcmp("--numeric-sort", argv[i]) == 0) {
            numeric_sort = true;
        } else {
            fprintf(stderr, "Invalid Option%s\n", argv[1]);
            return 1;
        }
    }
    
    scan_processes();
    build_tree();

    if (processes[1]) {
        print_tree(processes[1], 0);
    } else {
        printf("1 not found\n");
    }

    for (int i = 0; i < proc_count; i++) {
        free(all_processes[i]);
    }

    return 0;
}
