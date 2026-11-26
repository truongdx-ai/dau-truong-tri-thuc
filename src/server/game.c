#include "game.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int active_count(const Game *game) {
    int count = 0;
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        count += game->players[i].active && !game->players[i].departed;
    }
    return count;
}

static int best_player(const Game *game) {
    int best = -1;
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        const Player *player = &game->players[i];
        if (player->active && !player->departed &&
            (best < 0 || player->points > game->players[best].points)) {
            best = i;
        }
    }
    return best;
}

static void finish(Game *game, int winner, const char *reason) {
    char line[128];
    if (winner >= 0) {
        snprintf(line, sizeof(line), "GAME_OVER|WINNER|%s|%" PRId64 "\n",
                 game->players[winner].username, game->players[winner].points);
    } else {
        snprintf(line, sizeof(line), "GAME_OVER|%s\n", reason);
    }
    game->phase = GAME_IDLE;
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (game->players[i].username[0] != '\0') {
            game->emit(game->context, i, line);
        }
    }
}

static void send_question(Game *game) {
    const Question *question = &game->questions[game->question_index];
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        Player *player = &game->players[i];
        player->correct = player->responded = 0;
        player->answered_at = INT64_MAX;
        if (!player->active || player->departed) {
            continue;
        }
        unsigned seconds = game->phase == GAME_TURN && (int)i == game->main_slot ? 60 : 30;
        char line[1025];
        snprintf(line, sizeof(line), "QUESTION|%u|%s|%u|%s|%s|%s|%s|%s\n", game->qid,
                 game->phase == GAME_QUALIFY ? "QUALIFY" : "TURN", seconds, question->text,
                 question->options[0], question->options[1], question->options[2],
                 question->options[3]);
        game->emit(game->context, i, line);
    }
}

static void open_question(Game *game, int64_t now) {
    if (game->next_question == game->question_count) {
        finish(game, game->phase == GAME_QUALIFY ? -1 : best_player(game), "CANCELLED");
        return;
    }
    game->question_index = game->next_question++;
    ++game->qid;
    game->opened_at = now;
    game->skipped = 0;
    send_question(game);
}

static void send_roles(Game *game) {
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        if (game->players[i].active && !game->players[i].departed) {
            char line[64];
            if (i == game->main_slot) {
                snprintf(line, sizeof(line), "ROLE|MAIN\n");
            } else {
                snprintf(line, sizeof(line), "ROLE|SUB|%s\n",
                         game->players[game->main_slot].username);
            }
            game->emit(game->context, (size_t)i, line);
        }
    }
}

int game_start(Game *game, const Lobby *lobby, const Question *questions, size_t count, int64_t now,
               GameEmit emit, void *context) {
    if (game->phase != GAME_IDLE || questions == NULL || count == 0) {
        return -1;
    }
    unsigned previous_qid = game->qid;
    *game = (Game){.questions = questions,
                   .question_count = count,
                   .phase = GAME_QUALIFY,
                   .main_slot = -1,
                   .attempts = 1,
                   .qid = previous_qid,
                   .emit = emit,
                   .context = context};
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (lobby->sessions[i].joined) {
            Player *player = &game->players[i];
            memcpy(player->username, lobby->sessions[i].username, sizeof(player->username));
            player->active = 1;
            player->points = 10;
        }
    }
    if (game->phase == GAME_QUALIFY && active_count(game) < 2) {
        game->phase = GAME_IDLE;
        return -1;
    }
    open_question(game, now);
    return 0;
}

static int64_t deadline(const Game *game, size_t slot) {
    return game->opened_at +
           (game->phase == GAME_TURN && (int)slot == game->main_slot ? 60000 : 30000);
}

const char *game_answer(Game *game, size_t slot, unsigned qid, char answer, int64_t now) {
    Player *player = &game->players[slot];
    if (game->phase == GAME_IDLE || !player->active || player->departed) {
        return "403|NOT_ALLOWED\n";
    }
    if (qid != game->qid) {
        return "409|STALE_QUESTION\n";
    }
    if (now >= deadline(game, slot)) {
        return "408|TIMEOUT\n";
    }
    if (player->responded) {
        return "409|ALREADY_ANSWERED\n";
    }
    if (answer < 'A' || answer > 'D') {
        return "400|BAD_REQUEST\n";
    }
    player->responded = 1;
    player->answered_at = now;
    player->answer_order = ++game->response_order;
    player->correct = answer == game->questions[game->question_index].answer;
    return "200|RECEIVED\n";
}

const char *game_skip(Game *game, size_t slot, unsigned qid, int64_t now) {
    if (game->phase != GAME_TURN || (int)slot != game->main_slot || game->players[slot].departed) {
        return "403|NOT_MAIN\n";
    }
    Player *player = &game->players[slot];
    if (qid != game->qid) {
        return "409|STALE_QUESTION\n";
    }
    if (now >= deadline(game, slot)) {
        return "408|TIMEOUT\n";
    }
    if (player->responded) {
        return "409|ALREADY_ANSWERED\n";
    }
    if (player->skips == 2) {
        return "403|NO_SKIP_LEFT\n";
    }
    player->responded = 1;
    game->skipped = 1;
    return "200|OK\n";
}

static int all_responded(const Game *game) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (game->players[i].active && !game->players[i].responded) {
            return 0;
        }
    }
    return 1;
}

static void qualify(Game *game, int64_t now) {
    int fastest = score_fastest(game->players, -1);
    if (fastest < 0) {
        if (game->attempts++ == 3) {
            finish(game, -1, "CANCELLED");
        } else {
            open_question(game, now);
        }
        return;
    }
    game->phase = GAME_TURN;
    game->main_slot = fastest;
    send_roles(game);
    open_question(game, now);
}

static void evaluate(Game *game, int64_t now) {
    int previous = game->main_slot;
    ScoreAction action = game->skipped
                             ? SCORE_SKIP
                             : (game->players[previous].correct ? SCORE_CORRECT : SCORE_WRONG);
    int was_active[SESSION_LIMIT];
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        was_active[i] = game->players[i].active;
    }
    game->main_slot = score_apply(game->players, previous, action);
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (was_active[i] && !game->players[i].departed) {
            char line[128];
            snprintf(line, sizeof(line), "TURN_RESULT|%c|%s|%" PRId64 "\n",
                     game->questions[game->question_index].answer,
                     game->players[i].correct ? "CORRECT" : "WRONG", game->players[i].points);
            game->emit(game->context, i, line);
            if (!game->players[i].active) {
                game->emit(game->context, i, "ELIMINATED|WRONG_ANSWER\n");
            }
        }
    }
    if (active_count(game) < 2 || game->main_slot < 0) {
        finish(game, active_count(game) == 1 ? best_player(game) : -1, "NO_WINNER");
    } else {
        send_roles(game);
        open_question(game, now);
    }
}

void game_tick(Game *game, int64_t now) {
    if (game->phase == GAME_IDLE) {
        return;
    }
    if (game->phase == GAME_QUALIFY && active_count(game) < 2) {
        finish(game, active_count(game) == 1 ? best_player(game) : -1, "NO_WINNER");
        return;
    }
    if (game->phase == GAME_QUALIFY) {
        if (all_responded(game) || now >= game->opened_at + 30000) {
            qualify(game, now);
        }
    } else if (now >= game->opened_at + 30000 &&
               (game->players[game->main_slot].responded || now >= game->opened_at + 60000)) {
        evaluate(game, now);
    }
}

void game_leave(Game *game, size_t slot, int64_t now) {
    if (game->phase == GAME_IDLE || !game->players[slot].active) {
        return;
    }
    Player *player = &game->players[slot];
    player->departed = player->responded = 1;
    player->correct = 0;
    if ((int)slot == game->main_slot) {
        game->skipped = 0;
    }
    if (game->phase == GAME_QUALIFY) {
        player->active = 0;
    }
    game_tick(game, now);
}

int game_poll_timeout(const Game *game, int64_t now) {
    if (game->phase == GAME_IDLE) {
        return -1;
    }
    int64_t next = game->opened_at + 30000;
    if (game->phase == GAME_TURN && now >= next) {
        next = game->opened_at + 60000;
        if (game->players[game->main_slot].responded) {
            return 0;
        }
    }
    return now >= next ? 0 : (int)(next - now);
}
