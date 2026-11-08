#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "table.h"
#include "parse.h"
#include "correctness.h"

const Element elements[TOTAL_ELEMENTS] = {
    #define X(value, ox, count, elect, type) \
        [value] = { ox, count, elect, type, value },
        SYMBOLS
    #undef X
};

int get_number(char **ptr, int *output) {
    char c;
    while (**ptr >= '0' && **ptr <= '9') {
        c = **ptr;
        if((*output > INT_MAX/10) || (*output == INT_MAX/10 && (c-'0') > INT_MAX%10)) return -1;
        *output = *output * 10 + (c-'0');
        (*ptr)++;    // At the end, this points right after number ends
    }
    if(*output == 0) return -1;
    return 0;
}

const Element* get_element(char **ptr) {
    char el[3];
    // Here we assume we already now its mayus
    el[0] = **ptr;
    (*ptr)++; 
    // If element has 2 chars read, ifn end string
    if(**ptr >= 'a' && **ptr <= 'z') {
        el[1] = **ptr;
        (*ptr)++; 
    } else el[1] = '\0';
    el[2] = '\0';
    ElementSymbol sym = get_element_symbol(el);
    if (sym < 0 || sym >= TOTAL_ELEMENTS) {
        return NULL;
    }
    return &elements[sym];
}

int lexer(char *str, Token token_list[]) {
    char *ptr = str;
    int token_count = 0;
    
    while (*ptr != '\0') {
        if (token_count >= MAX_TOKENS - 1) {
            fprintf(stderr, "Error: Too much tokens\n");
            return -1;
        }
        int number = 0; const Element *elem;
        if (*ptr >= '0' && *ptr <= '9') {
            if(get_number(&ptr, &number) == -1) {
                fprintf(stderr, "Integer overflow or number 0\n");
                return 1;
            }
            token_list[token_count++] = (Token) { .type = T_NUMBER, .value.number = number};
        } else if (*ptr >= 'A' && *ptr <= 'Z') {
            elem = get_element(&ptr);
            if (elem == NULL) {
                fprintf(stderr, "Error: Invalid element\n");
                return -1;
            }
            token_list[token_count++] = (Token) { .type = T_ELEMENT, .value.elem = (Element*)elem};
        } else if (*ptr == ' ' || *ptr == '\t') {
            ptr++;
        } else if (*ptr == '(') {
            token_list[token_count++] = (Token) { .type = T_OPAREN };
            ptr++;
        } else if (*ptr == ')') {
            token_list[token_count++] = (Token) { .type = T_CPAREN };
            ptr++;
        } else if (*ptr == '*') {
            token_list[token_count++] = (Token) { .type = T_MULTIP };
            ptr++;
        } else if (*ptr == '^') {
            token_list[token_count++] = (Token) { .type = T_CHARGE };
            ptr++;
        } else if (*ptr == '-' || *ptr == '+') {
            token_list[token_count++] = (Token) { .type = T_SIGN, .value.sign = *ptr };
            ptr++;
        } else {
            fprintf(stderr, "Error: Undefined syntax '%c'\n", *ptr);
            return -1;
        }
    }
    token_list[token_count++] = (Token) { .type = T_END };
    return 1;
}

int main(void) {
    char buffer[MAX_CHAR + 1];
    Node *root;
    Token token_list[MAX_TOKENS];
    int index = 0;

    printf("Formula: ");

    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        if(lexer(buffer, token_list) == -1) return 1;
        root = parse_formula(token_list, &index);
        if(root == NULL) {
            fprintf(stderr, "Invalid formula\n");
            return 1;
        }
        if(check_correctness(root) == -1) {
            fprintf(stderr, "Invalid formula\n");
            free_tree(root);
            return 1;
        }
        print_tree(root, 0);
    } else {
        fprintf(stderr, "Error reading expression\n");
        return 1;
    }

    free_tree(root);

    return 0;
}