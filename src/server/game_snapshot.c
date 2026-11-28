#include "game.h"

#include <inttypes.h>
#include <stdio.h>

static void broadcast(Game *game, const char *line) {
    for (size_t slot = 0; slot < SESSION_LIMIT; ++slot) {
        if (game->players[slot].username[0] != '\0' && !game->players[slot].departed) {
            game->emit(game->context, slot, line);
        }
    }
}

void game_scoreboard(Game *game) {
    char line[128];
    snprintf(line, sizeof(line), "SCOREBOARD_BEGIN|%u\n", game->qid);
    broadcast(game, line);
    for (size_t slot = 0; slot < SESSION_LIMIT; ++slot) {
        const Player *player = &game->players[slot];
        if (player->username[0] == '\0') {
            continue;
        }
        const char *status =
            player->departed ? "DISCONNECTED" : (player->active ? "ACTIVE" : "ELIMINATED");
        snprintf(line, sizeof(line), "SCORE|%s|%" PRId64 "|%s\n", player->username, player->points,
                 status);
        broadcast(game, line);
    }
    snprintf(line, sizeof(line), "SCOREBOARD_END|%u\n", game->qid);
    broadcast(game, line);
}
