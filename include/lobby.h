#ifndef DTTT_LOBBY_H
#define DTTT_LOBBY_H

#include "session.h"

/* Return wire response lines, including LF. */
const char *lobby_join(Lobby *lobby, size_t slot);
const char *lobby_start(Lobby *lobby, size_t slot);
void lobby_leave(Lobby *lobby, size_t slot);
int lobby_host(const Lobby *lobby);
/* Write PLAYERS frame, or return -1 when capacity is insufficient. */
int lobby_players(const Lobby *lobby, char *output, size_t capacity);

#endif
