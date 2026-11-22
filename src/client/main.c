#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *host = "127.0.0.1", *port = "5500";
    int raw = 0;
    int option;
    while ((option = getopt(argc, argv, "rh:p:")) != -1) {
        if (option == 'r') {
            raw = 1;
        } else if (option == 'h') {
            host = optarg;
        } else if (option == 'p') {
            port = optarg;
        } else {
            return EXIT_FAILURE;
        }
    }
    if (optind != argc) {
        fprintf(stderr, "Usage: %s [-r] [-h HOST] [-p PORT]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (client_run(host, port, raw) < 0) {
        fputs("Client connection failed\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
