#include "question.h"
#include "linebuf.h"
#include "protocol.h"

#include <openssl/rand.h>
#include <stdint.h>
#include <string.h>

int question_shuffle(Question *questions, size_t count) {
    if (count > QUESTION_MAX_COUNT) {
        return -1;
    }
    for (size_t i = count; i > 1; --i) {
        uint32_t random, bound = (uint32_t)i;
        uint32_t threshold = (uint32_t)(-bound) % bound;
        do {
            if (RAND_bytes((unsigned char *)&random, sizeof(random)) != 1) {
                return -1;
            }
        } while (random < threshold);
        size_t index = random % bound;
        Question temporary = questions[i - 1];
        questions[i - 1] = questions[index];
        questions[index] = temporary;
    }
    return 0;
}

static int parse_question(char *line, Question *question) {
    ProtocolMessage message;
    if (protocol_parse(line, &message) < 0 || message.count != 6) {
        return -1;
    }

    const char *answer = message.fields[5];
    if (answer[0] < 'A' || answer[0] > 'D' || answer[1] != '\0' ||
        strlen(message.fields[0]) >= sizeof(question->text)) {
        return -1;
    }
    for (size_t i = 0; i < 4; ++i) {
        size_t length = strlen(message.fields[i + 1]);
        if (length >= sizeof(question->options[i])) {
            return -1;
        }
        memcpy(question->options[i], message.fields[i + 1], length + 1);
    }
    memcpy(question->text, message.fields[0], strlen(message.fields[0]) + 1);
    question->answer = answer[0];
    return 0;
}

int question_load(FILE *input, Question *questions, size_t capacity, size_t *count) {
    *count = 0;
    Linebuf line = {0};
    size_t loaded = 0;
    int byte;
    while ((byte = fgetc(input)) != EOF) {
        if (byte == '\0') {
            return -1;
        }
        LinebufResult result = linebuf_feed(&line, (char)byte);
        if (result == LINEBUF_TOO_LONG) {
            return -1;
        }
        if (result == LINEBUF_READY && line.data[0] != '\0') {
            if (loaded == capacity || parse_question(line.data, &questions[loaded]) < 0) {
                return -1;
            }
            ++loaded;
        }
    }
    if (ferror(input) || line.length != 0 || loaded == 0) {
        return -1;
    }
    *count = loaded;
    return 0;
}
