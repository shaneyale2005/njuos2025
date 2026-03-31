#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sperf_core.h"

extern char **environ;

static bool should_ignore_dtruss_line(const char *line) {
    return line[0] == '\0' ||
           strstr(line, "SYSCALL(args)") != NULL ||
           strstr(line, "ELAPSD") != NULL ||
           strstr(line, "PID/THRD") != NULL ||
           strncmp(line, "dtrace:", 7) == 0 ||
           strncmp(line, "dtruss:", 7) == 0 ||
           strncmp(line, "sudo:", 5) == 0;
}

static char *shell_quote(const char *arg) {
    if (!arg) {
        return NULL;
    }

    size_t extra = 2;
    for (const char *p = arg; *p; p++) {
        extra += (*p == '\'') ? 4 : 1;
    }

    char *quoted = malloc(extra + 1);
    if (!quoted) {
        return NULL;
    }

    char *out = quoted;
    *out++ = '\'';
    for (const char *p = arg; *p; p++) {
        if (*p == '\'') {
            memcpy(out, "'\\''", 4);
            out += 4;
        } else {
            *out++ = *p;
        }
    }
    *out++ = '\'';
    *out = '\0';
    return quoted;
}

int parse_dtruss_line(char *line, char *syscall_name, double *time) {
    if (!line || !syscall_name || !time) {
        return 0;
    }

    while (*line && isspace((unsigned char)*line)) {
        line++;
    }

    if (should_ignore_dtruss_line(line)) {
        return 0;
    }

    char *elapsed = line;
    if (!isdigit((unsigned char)*elapsed)) {
        return 0;
    }

    char *elapsed_end = NULL;
    long micros = strtol(elapsed, &elapsed_end, 10);
    if (!elapsed_end || elapsed_end == elapsed || micros < 0) {
        return 0;
    }

    while (*elapsed_end && isspace((unsigned char)*elapsed_end)) {
        elapsed_end++;
    }

    char *open_paren = strchr(elapsed_end, '(');
    if (!open_paren) {
        return 0;
    }

    int name_len = (int)(open_paren - elapsed_end);
    while (name_len > 0 && isspace((unsigned char)elapsed_end[name_len - 1])) {
        name_len--;
    }
    if (name_len <= 0 || name_len >= MAX_NAME_LEN) {
        return 0;
    }

    memcpy(syscall_name, elapsed_end, name_len);
    syscall_name[name_len] = '\0';
    *time = (double)micros / 1000000.0;
    return 1;
}

int spawn_macos_session(trace_session *session, int argc, char *argv[]) {
    if (!session || argc < 2) {
        return -1;
    }

    if (geteuid() != 0) {
        fprintf(stderr, "Error: dtruss requires root privileges on macOS; run with sudo\n");
        return -1;
    }

    char *dtruss_path = sperf_find_executable("dtruss");
    if (!dtruss_path) {
        fprintf(stderr, "Error: dtruss not found in PATH\n");
        return -1;
    }

    char *target_path = sperf_find_executable(argv[1]);
    if (!target_path) {
        fprintf(stderr, "Error: command not found: %s\n", argv[1]);
        free(dtruss_path);
        return -1;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        free(dtruss_path);
        free(target_path);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free(dtruss_path);
        free(target_path);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        int exec_argc = argc + 2;
        char **exec_argv = calloc((size_t)exec_argc + 1, sizeof(char *));
        if (!exec_argv) {
            perror("calloc");
            _exit(EXIT_FAILURE);
        }

        exec_argv[0] = dtruss_path;
        exec_argv[1] = "-e";
        exec_argv[2] = "-L";
        exec_argv[3] = shell_quote(target_path);
        if (!exec_argv[3]) {
            perror("malloc");
            _exit(EXIT_FAILURE);
        }

        for (int i = 2; i < argc; i++) {
            exec_argv[i + 2] = shell_quote(argv[i]);
            if (!exec_argv[i + 2]) {
                perror("malloc");
                _exit(EXIT_FAILURE);
            }
        }

        execve(dtruss_path, exec_argv, environ);
        perror("execve");
        _exit(EXIT_FAILURE);
    }

    close(pipefd[1]);
    session->pid = pid;
    session->read_fd = pipefd[0];
    session->parser = parse_dtruss_line;

    free(dtruss_path);
    free(target_path);
    return 0;
}
