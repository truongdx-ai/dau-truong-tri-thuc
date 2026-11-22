#ifndef DTTT_LOG_H
#define DTTT_LOG_H

#include <stdio.h>

typedef enum { LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

/* Write and flush a UTC timestamp, severity and single-line event. */
int log_event(FILE *stream, LogLevel level, const char *event);

#endif
