#ifndef DTTT_GAME_H
#define DTTT_GAME_H

#include "question.h"
#include "scoring.h"

typedef int (*GameEmit)(void *context, size_t slot, const char *line);
typedef enum { GAME_IDLE, GAME_QUALIFY, GAME_TURN } GamePhase;

typedef struct {
    Player players[SESSION_LIMIT];
    GamePhase phase;
    const Question *questions;
    size_t question_count;
    size_t next_question;
    size_t question_index;
    unsigned qid;
    unsigned attempts;
    uint64_t response_order;
    int main_slot;
    int skipped;
    int64_t opened_at;
    GameEmit emit;
    void *context;
} Game;

/* Caller supplies questions in their desired (possibly shuffled) order. */
int game_start(Game *game, const Lobby *lobby, const Question *questions, size_t count, int64_t now,
               GameEmit emit, void *context);
const char *game_answer(Game *game, size_t slot, unsigned qid, char answer, int64_t now);
const char *game_skip(Game *game, size_t slot, unsigned qid, int64_t now);
void game_tick(Game *game, int64_t now);
void game_leave(Game *game, size_t slot, int64_t now);
/* Milliseconds until the next game deadline, -1 while idle. */
int game_poll_timeout(const Game *game, int64_t now);

#endif
