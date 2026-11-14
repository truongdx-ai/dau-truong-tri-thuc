#include "protocol.h"
#include "linebuf.h"

#include <string.h>

int protocol_format(char *output, size_t capacity, const char *const fields[], size_t count) {
    if (count == 0 || count > PROTOCOL_MAX_FIELDS) {
        return -1;
    }
    size_t length = count;
    size_t sizes[PROTOCOL_MAX_FIELDS];
    for (size_t i = 0; i < count; ++i) {
        sizes[i] = strcspn(fields[i], "|\r\n");
        if (sizes[i] == 0 || sizes[i] > LINEBUF_MAX - length || fields[i][sizes[i]] != '\0') {
            return -1;
        }
        length += sizes[i];
    }
    if (capacity <= length) {
        return -1;
    }
    char *cursor = output;
    for (size_t i = 0; i < count; ++i) {
        memcpy(cursor, fields[i], sizes[i]);
        cursor += sizes[i];
        *cursor++ = i + 1 == count ? '\n' : '|';
    }
    *cursor = '\0';
    return (int)length;
}

int protocol_parse(char *line, ProtocolMessage *message) {
    message->count = 0;
    if (strpbrk(line, "\r\n") != NULL) {
        return -1;
    }

    ProtocolMessage parsed = {0};
    char *field = line;
    for (;;) {
        if (*field == '\0' || *field == '|' || parsed.count == PROTOCOL_MAX_FIELDS) {
            return -1;
        }
        parsed.fields[parsed.count++] = field;
        char *separator = strchr(field, '|');
        if (separator == NULL) {
            *message = parsed;
            return 0;
        }
        *separator = '\0';
        field = separator + 1;
    }
}
