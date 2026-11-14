#ifndef DTTT_LINEBUF_H
#define DTTT_LINEBUF_H

#include <stddef.h>

/* Maximum wire length, including LF (and optional CR). */
#define LINEBUF_MAX 1024

typedef struct {
    char data[LINEBUF_MAX];
    size_t length;
    int discarding;
} Linebuf;

typedef enum { LINEBUF_MORE, LINEBUF_READY, LINEBUF_TOO_LONG } LinebufResult;

/* Start with a zero-initialized buffer. Read data immediately on LINEBUF_READY. */
LinebufResult linebuf_feed(Linebuf *buffer, char byte);

#endif
