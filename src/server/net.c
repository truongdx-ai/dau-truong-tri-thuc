#include "net.h"
#include "linebuf.h"
#include "protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 100

typedef struct {
    Linebuf input;
    char output[LINEBUF_MAX + 1];
    size_t sent;
    size_t length;
    int invalid_input;
} Client;

static int nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int net_listen(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int reuse = 1;
    struct sockaddr_in address = {
        .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        nonblocking(fd) < 0 || bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(fd, MAX_CLIENTS) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

static void accept_client(int listener, struct pollfd *fds, Client *clients) {
    int fd = accept(listener, NULL, NULL);
    if (fd < 0) {
        return;
    }
    if (nonblocking(fd) == 0) {
        for (size_t i = 1; i <= MAX_CLIENTS; ++i) {
            if (fds[i].fd < 0) {
                fds[i] = (struct pollfd){.fd = fd, .events = POLLIN};
                clients[i - 1] = (Client){0};
                return;
            }
        }
    }
    close(fd);
}

static int retryable(void) { return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK; }

static void respond(Client *client, LinebufResult result) {
    ProtocolMessage message;
    const char *fields[] = {"400", "BAD_REQUEST"};
    size_t count = 2;
    if (result == LINEBUF_READY) {
        if (!client->invalid_input && protocol_parse(client->input.data, &message) == 0) {
            if (message.count == 1 && strcmp(message.fields[0], "PING") == 0) {
                fields[0] = "PONG";
                count = 1;
            } else {
                fields[1] = "UNSUPPORTED_COMMAND";
            }
        }
        client->invalid_input = 0;
    }
    client->length = (size_t)protocol_format(client->output, sizeof(client->output), fields, count);
    client->sent = 0;
}

static int service(struct pollfd *fd, Client *client) {
    if (fd->revents & (POLLERR | POLLNVAL)) {
        return -1;
    }
    if (fd->events == POLLOUT) {
        ssize_t n = send(fd->fd, client->output + client->sent, client->length - client->sent,
                         MSG_NOSIGNAL);
        if (n <= 0) {
            return n < 0 && retryable() ? 0 : -1;
        }
        client->sent += (size_t)n;
        if (client->sent == client->length) {
            fd->events = POLLIN;
        }
        return 0;
    }
    char byte;
    ssize_t n = recv(fd->fd, &byte, 1, 0);
    if (n <= 0) {
        return n < 0 && retryable() ? 0 : -1;
    }
    if (byte == '\0') {
        client->invalid_input = 1;
    }
    LinebufResult result = linebuf_feed(&client->input, byte);
    if (result != LINEBUF_MORE) {
        respond(client, result);
        fd->events = POLLOUT;
    }
    if (byte == '\n') {
        client->invalid_input = 0;
    }
    return 0;
}

int net_run(int listener, const volatile sig_atomic_t *stop) {
    struct pollfd fds[MAX_CLIENTS + 1];
    Client clients[MAX_CLIENTS];
    fds[0] = (struct pollfd){.fd = listener, .events = POLLIN};
    for (size_t i = 1; i <= MAX_CLIENTS; ++i) {
        fds[i].fd = -1;
    }
    int result = 0;
    while (!*stop) {
        if (poll(fds, MAX_CLIENTS + 1, 100) < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = -1;
            break;
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            result = -1;
            break;
        }
        for (size_t i = 1; i <= MAX_CLIENTS; ++i) {
            if (fds[i].fd >= 0 && fds[i].revents && service(&fds[i], &clients[i - 1]) < 0) {
                close(fds[i].fd);
                fds[i].fd = -1;
            }
        }
        if (fds[0].revents & POLLIN) {
            accept_client(listener, fds, clients);
        }
    }
    for (size_t i = 1; i <= MAX_CLIENTS; ++i) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd);
        }
    }
    return result;
}
