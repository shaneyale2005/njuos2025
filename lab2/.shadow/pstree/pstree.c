/* 实现思路
1. 遍历/proc获取信息
2. 构建父子关系
3. 打印进程树
4. 命令行参数解析 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>

#define MAX_CHILDREN 1024
#define MAX_PROCESSES 32768


// 在这里我们定义一个进程的结构体，这里要理解这个结构体内部有什么
typedef struct Process
{
    int pid;
    int ppid;
    char name[256];
    struct Process *children[MAX_CHILDREN];
    int child_count;
} Process;

// 这里存储的应该是所有的进程了
Process *processes[MAX_PROCESSES];
Process *all_processes[MAX_PROCESSES];
int proc_count = 0;

// 读取/proc目录获得pid，ppid，name这三个信息，这很重要的
Process *read_process(int pid) {
    char path[256];
    sprintf(path, "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    Process *p = (Process*)malloc(sizeof(Process));
    p->pid = pid;
    p->child_count = 0;

    // 这里写的话要理解stat的文件格式
    fscanf(fp, "%d (%[^)]) %*c %d", &p->pid, p->name, &p->ppid);
    fclose(fp);
    return p;
}

// 遍历/proc
void scan_processes(){
    DIR *dir = opendir("/proc");
    if (!dir) exit(1);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
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

// 递归打印树
void print_tree(Process *p, int level) {
    for (int i = 0; i < level; i++) {
        printf(" ");
    }
    printf("%s\n", p->name);
    for (int i = 0; i < p->child_count; i++) {
        print_tree(p->children[i], level + 1);
    }
}

// 构建父子关系
void build_tree() {
    for (int i = 0; i < proc_count; i++) {
        Process *p = all_processes[i];
        if (p->ppid > 0 && all_processes[p->ppid]) {
            Process *parent = processes[p->ppid];
            parent->children[parent->child_count++] = p;
        }

    }
}

int main(int argc, char *argv[]) {
    // 这里应该添加对命令行参数的解析部分
    if (argc > 1) {
        if (strcmp("-v", argv[1]) == 0 || strcmp("--version", argv[1]) == 0) {
            printf("This is my pstree\n");
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
