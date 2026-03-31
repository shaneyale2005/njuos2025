#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_SYSCALLS 1024
#define MAX_NAME_LEN 64
#define TOP_N 5
#define OUTPUT_INTERVAL_NS 100000000L
#define TRACE_FD 3

extern char **environ;

typedef struct {
    char name[MAX_NAME_LEN];
    double time;
} syscall_stat;

typedef struct {
    syscall_stat stats[MAX_SYSCALLS];
    int count;
    double total_time;
} syscall_stats;

static bool should_ignore_line(const char *line) {
    return line[0] == '\0' ||
           strncmp(line, "strace:", 7) == 0 ||
           strncmp(line, "+++ exited with ", 16) == 0 ||
           strncmp(line, "+++ killed by ", 14) == 0 ||
           strncmp(line, "--- SIG", 7) == 0 ||
           strstr(line, "<unfinished ...>") != NULL ||
           strstr(line, "resumed>") != NULL;
}

static int is_executable(const char *path) {
    return access(path, X_OK) == 0;
}

static char *join_path(const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    size_t need_slash = dir_len > 0 && dir[dir_len - 1] != '/';
    char *path = malloc(dir_len + need_slash + name_len + 1);

    if (!path) {
        return NULL;
    }

    memcpy(path, dir, dir_len);
    if (need_slash) {
        path[dir_len] = '/';
        memcpy(path + dir_len + 1, name, name_len + 1);
    } else {
        memcpy(path + dir_len, name, name_len + 1);
    }
    return path;
}

static char *find_executable(const char *command) {
    if (!command || command[0] == '\0') {
        return NULL;
    }

    if (strchr(command, '/')) {
        return is_executable(command) ? strdup(command) : NULL;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) {
        return NULL;
    }

    char *path_copy = strdup(path_env);
    if (!path_copy) {
        return NULL;
    }

    char *saveptr = NULL;
    for (char *dir = strtok_r(path_copy, ":", &saveptr);
         dir != NULL;
         dir = strtok_r(NULL, ":", &saveptr)) {
        const char *actual_dir = (*dir == '\0') ? "." : dir;
        char *candidate = join_path(actual_dir, command);
        if (candidate && is_executable(candidate)) {
            free(path_copy);
            return candidate;
        }
        free(candidate);
    }

    free(path_copy);
    return NULL;
}

int parse_strace_line(char *line, char *syscall_name, double *time) {
    if (!line || !syscall_name || !time) {
        return 0;
    }

    while (*line && isspace((unsigned char)*line)) {
        line++;
    }

    if (should_ignore_line(line)) {
        return 0;
    }

    char *open_paren = strchr(line, '(');
    if (!open_paren) {
        return 0;
    }

    int name_len = (int)(open_paren - line);
    while (name_len > 0 && isspace((unsigned char)line[name_len - 1])) {
        name_len--;
    }
    if (name_len <= 0 || name_len >= MAX_NAME_LEN) {
        return 0;
    }

    char *lt = strrchr(line, '<');
    char *gt = strrchr(line, '>');
    if (!lt || !gt || lt > gt) {
        return 0;
    }

    char time_buf[32];
    int time_len = (int)(gt - lt - 1);
    if (time_len <= 0 || time_len >= (int)sizeof(time_buf)) {
        return 0;
    }

    memcpy(time_buf, lt + 1, time_len);
    time_buf[time_len] = '\0';

    char *endptr = NULL;
    double parsed_time = strtod(time_buf, &endptr);
    if (!endptr || *endptr != '\0') {
        return 0;
    }

    memcpy(syscall_name, line, name_len);
    syscall_name[name_len] = '\0';
    *time = parsed_time;
    return 1;
}

void add_syscall(syscall_stats *stats, const char *name, double time) {
    if (!stats || !name || time < 0) {
        return;
    }

    for (int i = 0; i < stats->count; i++) {
        if (strcmp(stats->stats[i].name, name) == 0) {
            stats->stats[i].time += time;
            stats->total_time += time;
            return;
        }
    }

    if (stats->count >= MAX_SYSCALLS) {
        return;
    }

    strncpy(stats->stats[stats->count].name, name, MAX_NAME_LEN - 1);
    stats->stats[stats->count].name[MAX_NAME_LEN - 1] = '\0';
    stats->stats[stats->count].time = time;
    stats->count++;
    stats->total_time += time;
}

static int cmp_syscalls_desc(const void *lhs, const void *rhs) {
    const syscall_stat *a = lhs;
    const syscall_stat *b = rhs;

    if (a->time < b->time) {
        return 1;
    }
    if (a->time > b->time) {
        return -1;
    }
    return strcmp(a->name, b->name);
}

void print_top_syscalls(syscall_stats *stats, int n) {
    if (!stats) {
        return;
    }

    if (stats->count > 0 && stats->total_time > 0) {
        syscall_stat sorted[MAX_SYSCALLS];
        memcpy(sorted, stats->stats, sizeof(syscall_stat) * stats->count);
        qsort(sorted, stats->count, sizeof(syscall_stat), cmp_syscalls_desc);

        int limit = stats->count < n ? stats->count : n;
        for (int i = 0; i < limit; i++) {
            int ratio = (int)((sorted[i].time / stats->total_time) * 100.0);
            printf("%s (%d%%)\n", sorted[i].name, ratio);
        }
    }

    for (int i = 0; i < 80; i++) {
        putchar('\0');
    }
    fflush(stdout);
}

static long long monotonic_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int spawn_trace_reader(int pipefd[2], const char *strace_path,
                              const char *target_path, int argc, char *argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        char fd_path[32];
        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", TRACE_FD);

        close(pipefd[0]);
        if (pipefd[1] != TRACE_FD) {
            dup2(pipefd[1], TRACE_FD);
            close(pipefd[1]);
        }

        int exec_argc = argc + 4;
        char **exec_argv = calloc((size_t)exec_argc + 1, sizeof(char *));
        if (!exec_argv) {
            perror("calloc");
            _exit(EXIT_FAILURE);
        }

        exec_argv[0] = (char *)strace_path;
        exec_argv[1] = "-T";
        exec_argv[2] = "-o";
        exec_argv[3] = fd_path;
        exec_argv[4] = (char *)target_path;
        for (int i = 2; i < argc; i++) {
            exec_argv[i + 3] = argv[i];
        }

        execve(strace_path, exec_argv, environ);
        perror("execve");
        _exit(EXIT_FAILURE);
    }

    close(pipefd[1]);
    return pid;
}

static void process_trace_stream(int read_fd, syscall_stats *stats) {
    char chunk[4096];
    char linebuf[8192];
    size_t used = 0;
    long long last_print = 0;

    for (;;) {
        ssize_t nread = read(read_fd, chunk, sizeof(chunk));
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (nread == 0) {
            break;
        }

        for (ssize_t i = 0; i < nread; i++) {
            char ch = chunk[i];
            if (used + 1 < sizeof(linebuf)) {
                linebuf[used++] = ch;
            }

            if (ch != '\n') {
                continue;
            }

            linebuf[used - 1] = '\0';
            char syscall_name[MAX_NAME_LEN];
            double time = 0.0;
            if (parse_strace_line(linebuf, syscall_name, &time)) {
                add_syscall(stats, syscall_name, time);

                long long now = monotonic_now_ns();
                if (last_print == 0 || now - last_print >= OUTPUT_INTERVAL_NS) {
                    print_top_syscalls(stats, TOP_N);
                    last_print = now;
                }
            }
            used = 0;
        }
    }

    if (used > 0) {
        linebuf[used] = '\0';
        char syscall_name[MAX_NAME_LEN];
        double time = 0.0;
        if (parse_strace_line(linebuf, syscall_name, &time)) {
            add_syscall(stats, syscall_name, time);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s COMMAND [ARG]...\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *strace_path = find_executable("strace");
    if (!strace_path) {
        fprintf(stderr, "Error: strace not found in PATH\n");
        return EXIT_FAILURE;
    }

    char *target_path = find_executable(argv[1]);
    if (!target_path) {
        fprintf(stderr, "Error: command not found: %s\n", argv[1]);
        free(strace_path);
        return EXIT_FAILURE;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        free(strace_path);
        free(target_path);
        return EXIT_FAILURE;
    }

    int child = spawn_trace_reader(pipefd, strace_path, target_path, argc, argv);
    free(strace_path);
    free(target_path);
    if (child < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    syscall_stats stats = {0};
    process_trace_stream(pipefd[0], &stats);
    close(pipefd[0]);

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    print_top_syscalls(&stats, TOP_N);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return EXIT_FAILURE;
}
