#include <stdio.h>
#include <stdlib.h>

#include "sperf_core.h"

static int spawn_current_platform(trace_session *session, int argc, char *argv[]) {
#ifdef __APPLE__
    return spawn_macos_session(session, argc, argv);
#elif defined(__linux__)
    return spawn_linux_session(session, argc, argv);
#else
    (void)session;
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: unsupported platform\n");
    return -1;
#endif
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s COMMAND [ARG]...\n", argv[0]);
        return EXIT_FAILURE;
    }

    trace_session session = {
        .pid = -1,
        .read_fd = -1,
        .parser = NULL,
    };

    if (spawn_current_platform(&session, argc, argv) != 0) {
        return EXIT_FAILURE;
    }

    return run_trace_session(&session);
}
