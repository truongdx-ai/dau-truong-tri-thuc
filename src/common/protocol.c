#include "protocol.h"

#include <string.h>

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
