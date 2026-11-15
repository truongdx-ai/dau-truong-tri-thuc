#include "log.h"

#include <string.h>
#include <time.h>

int log_event(FILE *stream, LogLevel level, const char *event) {
    const char *levels[] = {"INFO", "WARN", "ERROR"};
    if ((unsigned)level >= sizeof(levels) / sizeof(levels[0]) || event[0] == '\0' ||
        strpbrk(event, "\r\n") != NULL) {
        return -1;
    }
    time_t now = time(NULL);
    struct tm utc;
    char timestamp[32];
    if (gmtime_r(&now, &utc) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        return -1;
    }
    if (fprintf(stream, "%s [%s] %s\n", timestamp, levels[level], event) < 0) {
        return -1;
    }
    return fflush(stream) == 0 ? 0 : -1;
}
