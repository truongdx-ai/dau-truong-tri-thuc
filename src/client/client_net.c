#include "client.h"
#include "linebuf.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int connect_server(const char *host, const char *port) {
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
    struct addrinfo *addresses;
    if (getaddrinfo(host, port, &hints, &addresses) != 0) {
        return -1;
    }
    int fd = -1;
    for (struct addrinfo *address = addresses; address != NULL; address = address->ai_next) {
        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd >= 0 && connect(fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        if (fd >= 0) {
            close(fd);
        }
        fd = -1;
    }
    freeaddrinfo(addresses);
    if (fd >= 0) {
        int flags = fcntl(fd, F_GETFL);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(fd);
            return -1;
        }
    }
    return fd;
}

static int64_t monotonic_seconds(void) {
    struct timespec now;
    return clock_gettime(CLOCK_MONOTONIC, &now) == 0 ? now.tv_sec : -1;
}

static int transient(void) { return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK; }

static int receive_lines(int fd, Linebuf *buffer, ClientUI *ui) {
    char bytes[4096];
    ssize_t count = recv(fd, bytes, sizeof(bytes), 0);
    if (count <= 0) {
        return count < 0 && transient() ? 1 : (int)count;
    }
    for (ssize_t i = 0; i < count; ++i) {
        LinebufResult result = linebuf_feed(buffer, bytes[i]);
        if (result == LINEBUF_READY) {
            ui_receive(ui, buffer->data, stdout);
        } else if (result == LINEBUF_TOO_LONG) {
            return -1;
        }
    }
    return 1;
}

static int read_input(char *output, size_t *length, Linebuf *input, int *invalid, ClientUI *ui) {
    char byte;
    ssize_t n = read(STDIN_FILENO, &byte, 1);
    if (n == 0 && input->length > 0 && !input->discarding) {
        /* Flush the last complete command when a pipe ends without LF. */
        byte = '\n';
        n = 1;
    }
    if (n <= 0) {
        return n < 0 && errno == EINTR ? 1 : (int)n;
    }
    *invalid |= byte == '\0';
    LinebufResult result = linebuf_feed(input, byte);
    if (result == LINEBUF_TOO_LONG) {
        ui_message("Lenh qua dai, toi da 1024 byte gom LF.");
    } else if (result == LINEBUF_READY) {
        if (*invalid) {
            ui_message("Lenh chua byte NUL, vui long nhap lai.");
        } else {
            int prepared = ui_prepare(ui, input->data, output, LINEBUF_MAX + 1, stdout);
            if (prepared < 0) {
                ui_message("Khong the tao lenh: vuot gioi han 1024 byte.");
            } else {
                *length = (size_t)prepared;
            }
        }
    }
    if (byte == '\n') {
        *invalid = 0;
    }
    return 1;
}

static int client_loop(int fd, int raw) {
    struct pollfd fds[] = {{.fd = fd, .events = POLLIN}, {.fd = STDIN_FILENO, .events = POLLIN}};
    Linebuf socket_input = {0}, input = {0};
    char output[LINEBUF_MAX + 1];
    ClientUI ui = {.raw = raw};
    size_t length = 0, sent = 0;
    int eof = 0;
    int invalid = 0;
    int64_t last_ping = monotonic_seconds();
    for (;;) {
        fds[0].events = POLLIN | (length > sent ? POLLOUT : 0);
        fds[1].events = !eof && length == sent ? POLLIN : 0;
        if (poll(fds, 2, 1000) < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (fds[0].revents & (POLLERR | POLLNVAL)) {
            return -1;
        }
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            int result = receive_lines(fd, &socket_input, &ui);
            if (result <= 0) {
                return result;
            }
        }
        if (fds[0].revents & POLLOUT) {
            ssize_t n = send(fd, output + sent, length - sent, MSG_NOSIGNAL);
            if (n < 0 && !transient()) {
                return -1;
            }
            if (n > 0) {
                sent += (size_t)n;
            }
        }
        if (!eof && length == sent && (fds[1].revents & (POLLIN | POLLHUP))) {
            length = sent = 0;
            int result = read_input(output, &length, &input, &invalid, &ui);
            if (result < 0) {
                return -1;
            }
            eof = result == 0;
            if (eof) {
                if (shutdown(fd, SHUT_WR) < 0) {
                    return -1;
                }
                fds[1].fd = -1;
            }
        }
        int64_t now = monotonic_seconds();
        if (!eof && length == sent && now - last_ping >= 60) {
            memcpy(output, "PING\n", 5);
            length = 5;
            sent = 0;
            last_ping = now;
        }
    }
}

int client_run(const char *host, const char *port, int raw) {
    int fd = connect_server(host, port);
    if (fd < 0) {
        return -1;
    }
    ui_help();
    int result = client_loop(fd, raw);
    close(fd);
    return result;
}
