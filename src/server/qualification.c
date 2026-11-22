#include "qualification.h"

#include <limits.h>

int qualification_start(Qualification *round, const Lobby *lobby, const Question *question,
                        uint64_t qid, int64_t now, int64_t duration) {
    if (qid == 0 || now < 0 || duration <= 0 || now > INT64_MAX - duration ||
        question->answer < 'A' || question->answer > 'D') {
        return -1;
    }
    Qualification next = {.phase = QUALIFY_OPEN,
                          .qid = qid,
                          .opened_at = now,
                          .deadline = now + duration,
                          .correct_answer = question->answer,
                          .main_slot = -1};
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        next.players[i].participating = lobby->sessions[i].joined;
    }
    *round = next;
    return 0;
}

QualificationAnswerResult qualification_answer(Qualification *round, size_t slot, uint64_t qid,
                                               char choice, int64_t now) {
    if (slot >= SESSION_LIMIT || !round->players[slot].participating || choice < 'A' ||
        choice > 'D' || now < round->opened_at) {
        return QUALIFY_NOT_ALLOWED;
    }
    if (qid != round->qid) {
        return QUALIFY_WRONG_QUESTION;
    }
    if (now >= round->deadline) {
        return QUALIFY_TIMEOUT;
    }
    if (round->players[slot].answered) {
        return QUALIFY_ALREADY_ANSWERED;
    }
    if (round->phase != QUALIFY_OPEN) {
        return QUALIFY_NOT_ALLOWED;
    }
    QualificationPlayer *player = &round->players[slot];
    player->answered = 1;
    player->choice = choice;
    player->received_at = now;
    player->order = ++round->next_order;
    return QUALIFY_RECEIVED;
}

void qualification_leave(Qualification *round, size_t slot) {
    if (slot >= SESSION_LIMIT) {
        return;
    }
    round->players[slot] = (QualificationPlayer){0};
    if (round->phase == QUALIFY_SELECTED && round->main_slot == (int)slot) {
        round->main_slot = -1;
        round->phase = QUALIFY_OPEN;
    }
}

QualificationResult qualification_finish(Qualification *round, int64_t now) {
    if (round->phase != QUALIFY_OPEN) {
        return QUALIFY_PENDING;
    }
    size_t participants = 0, answered = 0;
    int best = -1;
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        const QualificationPlayer *player = &round->players[i];
        if (!player->participating) {
            continue;
        }
        ++participants;
        answered += (size_t)player->answered;
        if (!player->answered || player->choice != round->correct_answer) {
            continue;
        }
        if (best < 0 || player->received_at < round->players[best].received_at ||
            (player->received_at == round->players[best].received_at &&
             player->order < round->players[best].order)) {
            best = (int)i;
        }
    }
    if (participants == 0) {
        round->phase = QUALIFY_IDLE;
        round->main_slot = -1;
        return QUALIFY_EMPTY_ROOM;
    }
    if (answered < participants && now < round->deadline) {
        return QUALIFY_PENDING;
    }
    round->main_slot = best;
    round->phase = best >= 0 ? QUALIFY_SELECTED : QUALIFY_NO_WINNER;
    return best >= 0 ? QUALIFY_MAIN_CHOSEN : QUALIFY_FINISHED_EMPTY;
}
