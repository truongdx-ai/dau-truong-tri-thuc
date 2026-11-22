#ifndef DTTT_QUALIFICATION_H
#define DTTT_QUALIFICATION_H

#include "question.h"
#include "session.h"

#define QUALIFICATION_DURATION_MS 30000

typedef enum { QUALIFY_IDLE, QUALIFY_OPEN, QUALIFY_SELECTED, QUALIFY_NO_WINNER } QualificationPhase;
typedef enum {
    QUALIFY_RECEIVED,
    QUALIFY_NOT_ALLOWED,
    QUALIFY_TIMEOUT,
    QUALIFY_ALREADY_ANSWERED,
    QUALIFY_WRONG_QUESTION
} QualificationAnswerResult;
typedef enum {
    QUALIFY_PENDING,
    QUALIFY_MAIN_CHOSEN,
    QUALIFY_FINISHED_EMPTY,
    QUALIFY_EMPTY_ROOM
} QualificationResult;

typedef struct {
    int participating;
    int answered;
    char choice;
    int64_t received_at;
    uint64_t order;
} QualificationPlayer;

typedef struct {
    QualificationPhase phase;
    uint64_t qid;
    int64_t opened_at;
    int64_t deadline;
    char correct_answer;
    int main_slot;
    uint64_t next_order;
    QualificationPlayer players[SESSION_LIMIT];
} Qualification;

int qualification_start(Qualification *round, const Lobby *lobby, const Question *question,
                        uint64_t qid, int64_t now, int64_t duration);
QualificationAnswerResult qualification_answer(Qualification *round, size_t slot, uint64_t qid,
                                               char choice, int64_t now);
void qualification_leave(Qualification *round, size_t slot);
/* Resolve once all remaining participants answered, or at the deadline. */
QualificationResult qualification_finish(Qualification *round, int64_t now);

#endif
