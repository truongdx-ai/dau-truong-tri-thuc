#include "net.h"
#include "question.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;

static void stop(int signal_number) {
    (void)signal_number;
    stopped = 1;
}

static Question *load_questions(const char *path, size_t *count) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return NULL;
    }
    Question *questions = calloc(QUESTION_MAX_COUNT, sizeof(*questions));
    int result = questions == NULL ? -1 : question_load(file, questions, QUESTION_MAX_COUNT, count);
    fclose(file);
    if (result < 0) {
        fprintf(stderr, "Could not load questions from %s\n", path);
        free(questions);
        return NULL;
    }
    if (question_shuffle(questions, *count) < 0) {
        fputs("Could not shuffle question bank\n", stderr);
        free(questions);
        return NULL;
    }
    return questions;
}

static int run_server(uint16_t port, const char *directory) {
    char users[PATH_MAX], questions[PATH_MAX], logs[PATH_MAX];
    if (snprintf(users, sizeof(users), "%s/users.db", directory) >= (int)sizeof(users) ||
        snprintf(questions, sizeof(questions), "%s/questions.txt", directory) >=
            (int)sizeof(questions) ||
        snprintf(logs, sizeof(logs), "%s/server.log", directory) >= (int)sizeof(logs)) {
        fputs("Data directory path is too long\n", stderr);
        return EXIT_FAILURE;
    }
    size_t question_count;
    Question *bank = load_questions(questions, &question_count);
    if (bank == NULL) {
        return EXIT_FAILURE;
    }
    FILE *log = fopen(logs, "a");
    if (log == NULL) {
        perror(logs);
        free(bank);
        return EXIT_FAILURE;
    }
    int fd = net_listen(port);
    int result = -1;
    if (fd < 0) {
        perror("listen");
    } else {
        NetConfig config = {
            .users_path = users, .log = log, .questions = bank, .question_count = question_count};
        result = net_run(fd, &stopped, &config);
        close(fd);
    }
    fclose(log);
    free(bank);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv) {
    long port = 5500;
    const char *directory = "data";
    int option;
    while ((option = getopt(argc, argv, "p:d:")) != -1) {
        if (option == 'p') {
            char *end;
            port = strtol(optarg, &end, 10);
            if (*end != '\0') {
                port = -1;
            }
        } else if (option == 'd') {
            directory = optarg;
        } else {
            return EXIT_FAILURE;
        }
    }
    if (port < 1 || port > UINT16_MAX || optind != argc || directory[0] == '\0') {
        fprintf(stderr, "Usage: %s [-p PORT] [-d DATA_DIR]\n", argv[0]);
        return EXIT_FAILURE;
    }
    struct sigaction action = {.sa_handler = stop};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 || sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    return run_server((uint16_t)port, directory);
}
