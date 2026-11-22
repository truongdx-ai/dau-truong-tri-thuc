#ifndef DTTT_PROTOCOL_H
#define DTTT_PROTOCOL_H

#include <stddef.h>

#define PROTOCOL_MAX_FIELDS 9

typedef struct {
    char *fields[PROTOCOL_MAX_FIELDS];
    size_t count;
} ProtocolMessage;

/* Split a line in place. Returns 0 on success, -1 on malformed input. */
int protocol_parse(char *line, ProtocolMessage *message);

/* Return the frame length including LF, or -1 if it cannot be encoded. */
int protocol_format(char *output, size_t capacity, const char *const fields[], size_t count);

#endif
