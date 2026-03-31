#ifndef SPERF_CORE_H
#define SPERF_CORE_H

#include <stdbool.h>
#include <sys/types.h>

#define MAX_SYSCALLS 1024
#define MAX_NAME_LEN 64
#define TOP_N 5

typedef struct {
    char name[MAX_NAME_LEN];
    double time;
} syscall_stat;

typedef struct {
    syscall_stat stats[MAX_SYSCALLS];
    int count;
    double total_time;
} syscall_stats;

typedef int (*trace_parser_fn)(char *line, char *syscall_name, double *time);

typedef struct {
    pid_t pid;
    int read_fd;
    trace_parser_fn parser;
} trace_session;

char *sperf_find_executable(const char *command);
void add_syscall(syscall_stats *stats, const char *name, double time);
void print_top_syscalls(syscall_stats *stats, int n);
int run_trace_session(trace_session *session);

int parse_strace_line(char *line, char *syscall_name, double *time);

int spawn_linux_session(trace_session *session, int argc, char *argv[]);

#endif
