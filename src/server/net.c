#include "net.h"
#include "dispatcher.h"
#include "linebuf.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define OUTPUT_CAPACITY 16384

typedef struct {
    Linebuf input;
    char output[OUTPUT_CAPACITY];
    size_t length;
    size_t sent;
    int invalid;
    int failed;
    int eof;
} Client;

typedef struct {
    struct pollfd fds[SESSION_LIMIT + 1];
    Client clients[SESSION_LIMIT];
    Dispatcher dispatcher;
} Server;

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
        listen(fd, SESSION_LIMIT) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

static int emit(void *context, size_t slot, const char *line) {
    Server *server = context;
    Client *client = &server->clients[slot];
    size_t length = strlen(line);
    size_t pending = client->length - client->sent;
    if (length > sizeof(client->output) - pending) {
        client->failed = 1;
        return -1;
    }
    memmove(client->output, client->output + client->sent, pending);
    memcpy(client->output + pending, line, length);
    client->length = pending + length;
    client->sent = 0;
    server->fds[slot + 1].events |= POLLOUT;
    return 0;
}

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return -1;
    }
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int64_t now_ms(const NetConfig *config) { return config->now_ms(); }

static int retryable(void) { return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK; }

static int receive_client(Server *server, size_t slot) {
    Client *client = &server->clients[slot];
    char bytes[4096];
    ssize_t n = recv(server->fds[slot + 1].fd, bytes, sizeof(bytes), 0);
    if (n < 0) {
        return retryable() ? 0 : -1;
    }
    if (n == 0) {
        client->eof = 1;
        return 0;
    }
    for (ssize_t i = 0; i < n; ++i) {
        client->invalid |= bytes[i] == '\0';
        LinebufResult result = linebuf_feed(&client->input, bytes[i]);
        if (result == LINEBUF_TOO_LONG || (result == LINEBUF_READY && client->invalid)) {
            emit(server, slot, "400|BAD_REQUEST\n");
        } else if (result == LINEBUF_READY) {
            /* Timestamp each complete command when the server processes its final LF. */
            int64_t now = now_ms(server->dispatcher.config);
            if (now < 0) {
                return -1;
            }
            dispatcher_command(&server->dispatcher, slot, client->input.data, now);
        }
        if (bytes[i] == '\n') {
            client->invalid = 0;
        }
        if (client->failed) {
            return -1;
        }
    }
    return 0;
}

static int send_client(Server *server, size_t slot) {
    Client *client = &server->clients[slot];
    ssize_t n = send(server->fds[slot + 1].fd, client->output + client->sent,
                     client->length - client->sent, MSG_NOSIGNAL);
    if (n <= 0) {
        return n < 0 && retryable() ? 0 : -1;
    }
    client->sent += (size_t)n;
    if (client->sent == client->length) {
        client->sent = client->length = 0;
        server->fds[slot + 1].events &= ~POLLOUT;
    }
    return 0;
}

static void disconnect_client(Server *server, size_t slot, int64_t now) {
    close(server->fds[slot + 1].fd);
    server->fds[slot + 1].fd = -1;
    server->clients[slot] = (Client){0};
    dispatcher_disconnect(&server->dispatcher, slot, now);
}

static void accept_client(Server *server) {
    int fd = accept(server->fds[0].fd, NULL, NULL);
    if (fd < 0) {
        return;
    }
    if (nonblocking(fd) == 0) {
        for (size_t i = 0; i < SESSION_LIMIT; ++i) {
            if (server->fds[i + 1].fd < 0) {
                server->fds[i + 1] = (struct pollfd){.fd = fd, .events = POLLIN};
                return;
            }
        }
    }
    close(fd);
}

static void service_clients(Server *server, int64_t timeout) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        struct pollfd *fd = &server->fds[i + 1];
        if (fd->fd < 0) {
            continue;
        }
        Client *client = &server->clients[i];
        int failed = client->failed || (fd->revents & (POLLERR | POLLNVAL));
        if (!failed && (fd->revents & POLLOUT)) {
            failed = send_client(server, i) < 0;
        }
        if (!failed && !client->eof && (fd->revents & (POLLIN | POLLHUP))) {
            failed = receive_client(server, i) < 0;
        }
        int64_t now = now_ms(server->dispatcher.config);
        if (now < 0) {
            failed = 1;
        }
        int expired = session_expired(&server->dispatcher.lobby.sessions[i], now, timeout);
        if (expired && log_event(server->dispatcher.config->log, LOG_INFO, "SESSION expired") < 0) {
            fputs("Could not write expiration log\n", stderr);
        }
        if (failed || (client->eof && client->length == 0) || expired) {
            disconnect_client(server, i, now);
        } else if (client->eof) {
            fd->events &= ~POLLIN;
        }
    }
}

int net_run(int listener, const volatile sig_atomic_t *stop, const NetConfig *config) {
    /* Share the same clock with command handling, including slow authentication. */
    NetConfig runtime = *config;
    if (runtime.now_ms == NULL) {
        runtime.now_ms = monotonic_ms;
    }
    config = &runtime;
    Server *server = calloc(1, sizeof(*server));
    if (server == NULL) {
        return -1;
    }
    server->dispatcher = (Dispatcher){.config = config, .emit = emit, .context = server};
    server->fds[0] = (struct pollfd){.fd = listener, .events = POLLIN};
    for (size_t i = 1; i <= SESSION_LIMIT; ++i) {
        server->fds[i].fd = -1;
    }
    int result = 0;
    int64_t timeout = config->idle_timeout_ms > 0 ? config->idle_timeout_ms : SESSION_IDLE_MS;
    while (!*stop) {
        if (poll(server->fds, SESSION_LIMIT + 1, 100) < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = -1;
            break;
        }
        if (server->fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            result = -1;
            break;
        }
        service_clients(server, timeout);
        int64_t now = now_ms(config);
        if (now < 0) {
            result = -1;
            break;
        }
        dispatcher_tick(&server->dispatcher, now);
        if (server->fds[0].revents & POLLIN) {
            accept_client(server);
        }
    }
    for (size_t i = 1; i <= SESSION_LIMIT; ++i) {
        if (server->fds[i].fd >= 0) {
            close(server->fds[i].fd);
        }
    }
    free(server);
    return result;
}
