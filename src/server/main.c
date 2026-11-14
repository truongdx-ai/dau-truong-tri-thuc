#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;

static void stop(int signal_number) {
    (void)signal_number;
    stopped = 1;
}

int main(int argc, char **argv) {
    long port = 5500;
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        char *end;
        port = strtol(argv[2], &end, 10);
        if (*end != '\0' || port < 1 || port > UINT16_MAX) {
            port = -1;
        }
    } else if (argc != 1) {
        port = -1;
    }
    if (port < 0) {
        fprintf(stderr, "Usage: %s [-p PORT]\n", argv[0]);
        return EXIT_FAILURE;
    }
    struct sigaction action = {.sa_handler = stop};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 || sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    int fd = net_listen((uint16_t)port);
    if (fd < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }
    int status = net_run(fd, &stopped);
    close(fd);
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
