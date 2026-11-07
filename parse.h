#ifndef PARSE_H
#define PARSE_H

#define MAX_CHAR 100
#define MAX_TOKENS 100

#include "table.h"

typedef enum {
    T_ELEMENT, T_NUMBER, T_OPAREN,
    T_CPAREN, T_MULTIP, T_END,
    T_CHARGE, T_SIGN, T_UNDEF
} TokenType;

typedef struct {
    TokenType type;
    union {
        int number;
        Element *elem;
        char sign;
        // Other cases, nothing
    } value;
} Token;

typedef enum {
    N_GROUP,
    N_ELEMENT,
    N_COMPOUND,
    N_ADDUCT
} NodeType;

typedef struct Node {
    union {
        Element *elem;
        struct Node **child;
    } data;
    NodeType type;
    int charge;
    int mult;
    int child_count;
    char charge_done; // To check if charge was already calculated 
} Node;

void print_tree(Node*, int);
void free_tree(Node*);
Node* parse_expression(Token[], int*);
Node* parse_formula(Token[], int*);
Node *parse_compound(Token[], int*);

#endif