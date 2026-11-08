#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "table.h"
#include "parse.h"

const Element elements[TOTAL_ELEMENTS] = {
    #define X(value, ox, count, elect, type) \
        [value] = { ox, count, elect, type, value },
        SYMBOLS
    #undef X
};

typedef struct {
    Node *node;
    int total_mult;
} UnknownX;

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

int fix_charges(Node *node) {
    if(node == NULL) return 1;
    if(node->type == N_ELEMENT) {
        Element* elem = node->data.elem;
        if(!node->charge_done) {
            if(elem->ox_state_count == 1) {
                node->charge = elem->ox_state[0];
                node->charge_done = 1;
            }
        } else { // Check if correct
            int ox = node->charge, j;
            for(j = 0; j < elem->ox_state_count; j++){
                if(ox == elem->ox_state[j]) break;
            }
            if(j == elem->ox_state_count) return -1;
        }
        return 1;
    }
    // Etc, all nodes with child
    int i;
    for(i = 0; i <node->child_count; i++) {
        if(fix_charges(node->data.child[i]) == -1) return -1;
    }
    return 1;
}

Node* resolve_electronegativity(Node *node) {
    if(node == NULL) return NULL;
    if(node->type == N_ELEMENT) return node;
    int max = -999;
    Node *chosen = NULL;
    for(int i = 0; i < node->child_count; i++) {
        Node *child = resolve_electronegativity(node->data.child[i]);
        if(child != NULL && child->type == N_ELEMENT) {
            Element *elem = child->data.elem;
            if(elem->electronegativity > max) {
                chosen = child;
                max = elem->electronegativity;
            }
        }
    }
    return chosen;
}

void pre_process(Node *node, int mult, Node *most_eln, int *obj_charge, int *count, UnknownX *inc) {
    if(node == NULL) return;

    int cur_mult = mult * node->mult;

    if(node->type == N_ELEMENT) {
        if(most_eln != NULL && node->data.elem == most_eln->data.elem && node->charge_done == 0) {
            node->charge_done = 1;
            node->charge = most_eln->charge;
        }
        if(node->charge_done) {
            *obj_charge -= (node->charge * cur_mult);
        } else { //Unknown
            inc[*count].node = node;
            inc[*count].total_mult = cur_mult;
            (*count)++;
        }
        return;
    }

    for(int i = 0; i < node->child_count; i++) {
        pre_process(node->data.child[i], cur_mult, most_eln, obj_charge, count, inc);
    }
}

int get_charge_group(Node *node) {
    if(node == NULL) return -1;
    if(node->type == N_ELEMENT) return node->charge;
    int charge = 0;
    for(int i = 0; i < node->child_count; i++) {
        charge += get_charge_group(node->data.child[i]) * node->data.child[i]->mult;
    }
    node->charge = charge;
    return charge;
}

float get_electronegativity(Node *node) {
    if(node->type == N_ELEMENT) return node->data.elem->electronegativity;
    if(node->child_count > 0) return get_electronegativity(node->data.child[0]);
    return -1;
}

char *get_symbol(Node *node) {
    if(node->type == N_ELEMENT) {
        return (char*)symbol_to_string(node->data.elem->symbol);
    }
    if(node->child_count > 0) return get_symbol(node->data.child[0]);
    return NULL;
}

int check_order(Node *node) {
    if(node == NULL || node->type == N_ELEMENT) return 1;
    for (int i = 0; i < node->child_count; i++) {
        if (check_order(node->data.child[i]) == -1) return -1;
    }
    for(int i = 0; i < node->child_count - 1; i++) {
        Node *c1 = node->data.child[i];
        Node *c2 = node->data.child[i+1];

        if(node->type == N_ADDUCT) {
            char *sym1 = get_symbol(c1);
            char *sym2 = get_symbol(c2);
            if (sym1 && sym2 && strcmp(sym1, sym2) > 0) return -1;
        } else {
            int ch1 = get_charge_group(c1);
            int ch2 = get_charge_group(c2);

            if (ch1 < ch2) return -1; 
            if (ch1 == ch2) {
                if(node->type == N_COMPOUND) {
                    char *sym1 = get_symbol(c1);
                    char *sym2 = get_symbol(c2);
                    if (sym1 && sym2 && strcmp(sym1, sym2) > 0) return -1;
                } else if(node->type == N_GROUP) {
                    float en1 = get_electronegativity(c1);
                    float en2 = get_electronegativity(c2);
                    if (en1 > en2) return -1;
                }
            }
        }
    }
    return 1;
}

int complete_charges(UnknownX *inc, int index, int max, int obj_charge, Node *root) {
    if(index == max) {
        if(obj_charge != 0) return -1;
        int ordered = check_order(root); 
        if(ordered == -1) return -1;
        return 1;
    }
    Element *elem = inc[index].node->data.elem;
    for(int i = 0; i < elem->ox_state_count; i++) {
        int charge = elem->ox_state[i] * inc[index].total_mult;
        inc[index].node->charge = elem->ox_state[i];
        if(complete_charges(inc, index+1, max, obj_charge - charge, root) == 1) return 1;
    }
    return -1;
}

int resolve_nodes(Node *node) {
    if(node == NULL) return 1;

    if(node->type == N_ELEMENT) {
        node->charge_done = 1;
        return 1;
    }

    int charge = 0;
    for(int i =0; i < node->child_count; i++) {
        resolve_nodes(node->data.child[i]);
        charge += (node->data.child[i]->charge * node->data.child[i]->mult);
    }
    node->charge = charge;
    node->charge_done = 1;
    return 1;
}

int has_mult_elements(Node *node, Element **first_seen) {
    if(node == NULL) return 0;
    if(node->type == N_ELEMENT) {
        if(*first_seen == NULL) {
            *first_seen = node->data.elem;
            return 0;
        } else if(*first_seen != node->data.elem) return 1;
        return 0;
    }
    for(int i = 0; i < node->child_count; i++) {
        if(has_mult_elements(node->data.child[i], first_seen) == 1) return 1;
    }
    return 0;
}

int calculate_charges(Node *node) { // Returns 0 if did nothing, 1 if done, -1 if error
    if(node->type == N_ADDUCT) { // First separate adducts
        int total_charge = 0;
        for(int i = 0; i < node->child_count; i++) {
            if(calculate_charges(node->data.child[i]) == -1) return -1;
            total_charge += (node->data.child[i]->charge * node->data.child[i]->mult);
        }
        node->charge = total_charge;
        node->charge_done = 1;
        return 1;
    }
    // Fill most eln from each adduct
    Node *most_eln = NULL;
    Element *first_seen = NULL;
    if(has_mult_elements(node, &first_seen) == 1) {
        most_eln = resolve_electronegativity(node);
    }
    
    if(most_eln != NULL) {
        most_eln->charge_done = 1;
        int min = 999;
        Element *el = most_eln->data.elem;
        for(int i = 0; i < el->ox_state_count; i++) {
            if(el->ox_state[i] < min) min = el->ox_state[i];
        }
        most_eln->charge = min;
    }

    // Pre-process before bting
    int obj_charge = node->charge, count_x = 0;
    UnknownX *inc = (UnknownX *)malloc(sizeof(UnknownX)*64); //Cap 64
    pre_process(node, 1, most_eln, &obj_charge, &count_x, inc);
    
    // Now check if correct
    int done = complete_charges(inc, 0, count_x, obj_charge, node);
    if(done == -1) {
        free(inc);
        fprintf(stderr, "Incorrect order/charges\n");
        return -1;
    } else { // Done
        int resolve = resolve_nodes(node);
        if(resolve == -1) return -1;
    }

    free(inc);
    return done;
}

int check_correctness(Node *root) {
    // First, check fix charges (In case of just one ox_state)
    if(fix_charges(root) == -1) {
        fprintf(stderr, "Incorrect charge in formula\n");
        return -1;
    }
    // Then, calculate final charges
    if(calculate_charges(root) == -1) {
        fprintf(stderr, "Can't calculate charges\n");
        return -1;
    }
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