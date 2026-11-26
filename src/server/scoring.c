#include "scoring.h"

int score_fastest(const Player players[SESSION_LIMIT], int excluded) {
    int fastest = -1;
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        if (i != excluded && players[i].active && players[i].correct &&
            (fastest < 0 || players[i].answered_at < players[fastest].answered_at ||
             (players[i].answered_at == players[fastest].answered_at &&
              players[i].answer_order < players[fastest].answer_order))) {
            fastest = i;
        }
    }
    return fastest;
}

static void distribute(Player players[SESSION_LIMIT], int main_slot, int fastest, int64_t pool) {
    int count = 0;
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        count += i != main_slot && players[i].active && players[i].correct;
    }
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        if (i != main_slot && players[i].active && players[i].correct) {
            players[i].points += pool / count;
        }
    }
    players[fastest].points += pool % count;
}

int score_apply(Player players[SESSION_LIMIT], int main_slot, ScoreAction action) {
    Player *main = &players[main_slot];
    int fastest = score_fastest(players, main_slot);
    int64_t pool = 0;
    if (action == SCORE_SKIP) {
        ++main->skips;
        if (fastest >= 0) {
            pool = main->points / 2;
            main->points -= pool;
        }
    } else if (action == SCORE_WRONG) {
        pool = main->points;
        main->active = 0;
        main->points = 0;
    }
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        if (i != main_slot && players[i].active && !players[i].correct) {
            if (action != SCORE_WRONG) {
                pool += players[i].points;
            }
            players[i].active = 0;
            players[i].points = 0;
        }
    }
    if (action == SCORE_CORRECT) {
        main->points += pool;
    } else if (fastest >= 0) {
        distribute(players, main_slot, fastest, pool);
    }
    return action == SCORE_WRONG ? fastest : main_slot;
}
