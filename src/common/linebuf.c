#include "linebuf.h"

LinebufResult linebuf_feed(Linebuf *buffer, char byte) {
    if (byte == '\n') {
        if (buffer->discarding) {
            buffer->discarding = 0;
            return LINEBUF_MORE;
        }
        if (buffer->length > 0 && buffer->data[buffer->length - 1] == '\r') {
            --buffer->length;
        }
        buffer->data[buffer->length] = '\0';
        buffer->length = 0;
        return LINEBUF_READY;
    }
    if (buffer->discarding) {
        return LINEBUF_MORE;
    }
    if (buffer->length == LINEBUF_MAX - 1) {
        buffer->length = 0;
        buffer->discarding = 1;
        return LINEBUF_TOO_LONG;
    }
    buffer->data[buffer->length++] = byte;
    return LINEBUF_MORE;
}
