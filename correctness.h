#ifndef CORRECTNESS_H
#define CORRECTNESS_H

#include "parse.h"

typedef struct {
    Node *node;
    int total_mult;
} UnknownX; // temporary type

int check_correctness(Node *);

#endif