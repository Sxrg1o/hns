#ifndef BALANCE_H
#define BALANCE_H

#include "parse.h"

#define MAX_VARIABLES 15

typedef struct {
    int num;
    int den;
} Fraction;

void balance_equation(Node *);

#endif