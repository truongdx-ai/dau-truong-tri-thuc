#ifndef DTTT_QUESTION_H
#define DTTT_QUESTION_H

#include <stddef.h>
#include <stdio.h>

#define QUESTION_TEXT_CAPACITY 384
#define QUESTION_OPTION_CAPACITY 128
#define QUESTION_MAX_COUNT 1000

typedef struct {
    char text[QUESTION_TEXT_CAPACITY];
    char options[4][QUESTION_OPTION_CAPACITY];
    char answer;
} Question;

/* Read text|A|B|C|D|correct-letter records. On failure, count is zero. */
int question_load(FILE *input, Question *questions, size_t capacity, size_t *count);
/* Shuffle in place. Return -1 if the random source fails or count is unsupported. */
int question_shuffle(Question *questions, size_t count);

#endif
