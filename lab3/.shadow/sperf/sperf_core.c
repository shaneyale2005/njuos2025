#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "sperf_core.h"

#define OUTPUT_INTERVAL_NS 100000000L

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

char *sperf_find_executable(const char *command) {
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

static void process_trace_stream(int read_fd, syscall_stats *stats, trace_parser_fn parser) {
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
            if (parser && parser(linebuf, syscall_name, &time)) {
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
        if (parser && parser(linebuf, syscall_name, &time)) {
            add_syscall(stats, syscall_name, time);
        }
    }
}

int run_trace_session(trace_session *session) {
    if (!session || session->read_fd < 0 || session->pid < 0 || !session->parser) {
        return EXIT_FAILURE;
    }

    syscall_stats stats = {0};
    process_trace_stream(session->read_fd, &stats, session->parser);
    close(session->read_fd);

    int status = 0;
    if (waitpid(session->pid, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    print_top_syscalls(&stats, TOP_N);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return EXIT_FAILURE;
}
