#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *host = "127.0.0.1", *port = "5500";
    int option;
    while ((option = getopt(argc, argv, "h:p:")) != -1) {
        if (option == 'h') {
            host = optarg;
        } else if (option == 'p') {
            port = optarg;
        } else {
            return EXIT_FAILURE;
        }
    }
    if (optind != argc) {
        fprintf(stderr, "Usage: %s [-h HOST] [-p PORT]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (client_run(host, port) < 0) {
        fputs("Client connection failed\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
