#ifndef DTTT_SCORING_H
#define DTTT_SCORING_H

#include "session.h"

typedef struct {
    char username[33];
    int active;
    int correct;
    int responded;
    int departed;
    int64_t answered_at;
    int64_t points;
    unsigned skips;
} Player;

typedef enum { SCORE_CORRECT, SCORE_WRONG, SCORE_SKIP } ScoreAction;

/* Resolve one turn, return the next main slot or -1 if nobody survives. */
int score_apply(Player players[SESSION_LIMIT], int main_slot, ScoreAction action);
int score_fastest(const Player players[SESSION_LIMIT], int excluded);

#endif
