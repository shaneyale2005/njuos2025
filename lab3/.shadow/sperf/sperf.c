#include <stdio.h>
#include <stdlib.h>

#include "sperf_core.h"

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

#ifdef __linux__
    if (spawn_linux_session(&session, argc, argv) != 0) {
        return EXIT_FAILURE;
    }

    return run_trace_session(&session);
#else
    (void)session;
    (void)argv;
    fprintf(stderr, "Error: sperf only supports Linux\n");
    return EXIT_FAILURE;
#endif
}
