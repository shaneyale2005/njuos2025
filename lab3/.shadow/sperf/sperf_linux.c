#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sperf_core.h"

#define TRACE_FD 3

extern char **environ;

static bool should_ignore_strace_line(const char *line) {
    return line[0] == '\0' ||
           strncmp(line, "strace:", 7) == 0 ||
           strncmp(line, "+++ exited with ", 16) == 0 ||
           strncmp(line, "+++ killed by ", 14) == 0 ||
           strncmp(line, "--- SIG", 7) == 0 ||
           strstr(line, "<unfinished ...>") != NULL ||
           strstr(line, "resumed>") != NULL;
}

int parse_strace_line(char *line, char *syscall_name, double *time) {
    if (!line || !syscall_name || !time) {
        return 0;
    }

    while (*line && isspace((unsigned char)*line)) {
        line++;
    }

    if (should_ignore_strace_line(line)) {
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

int spawn_linux_session(trace_session *session, int argc, char *argv[]) {
    if (!session || argc < 2) {
        return -1;
    }

    char *strace_path = sperf_find_executable("strace");
    if (!strace_path) {
        fprintf(stderr, "Error: strace not found in PATH\n");
        return -1;
    }

    char *target_path = sperf_find_executable(argv[1]);
    if (!target_path) {
        fprintf(stderr, "Error: command not found: %s\n", argv[1]);
        free(strace_path);
        return -1;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        free(strace_path);
        free(target_path);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free(strace_path);
        free(target_path);
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

        exec_argv[0] = strace_path;
        exec_argv[1] = "-T";
        exec_argv[2] = "-o";
        exec_argv[3] = fd_path;
        exec_argv[4] = target_path;
        for (int i = 2; i < argc; i++) {
            exec_argv[i + 3] = argv[i];
        }

        execve(strace_path, exec_argv, environ);
        perror("execve");
        _exit(EXIT_FAILURE);
    }

    close(pipefd[1]);
    session->pid = pid;
    session->read_fd = pipefd[0];
    session->parser = parse_strace_line;

    free(strace_path);
    free(target_path);
    return 0;
}
