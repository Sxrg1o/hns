#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "correctness.h"
#include "parse.h"

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
    float max = -999.0f;
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

ElementSymbol get_element_enum(Node *node) {
    if(node == NULL) return UNKNOWN_ELEMENT;
    if(node->type == N_ELEMENT) {
        return node->data.elem->symbol;
    }
    if(node->child_count > 0) {
        return get_element_enum(node->data.child[0]);
    }
    return UNKNOWN_ELEMENT;
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

            if(ch1 < ch2) {
                ElementSymbol s1 = get_element_enum(c1);
                ElementSymbol s2 = get_element_enum(c2);
                // esto es mierda porque la quimica es mierda no entiendo
                int is_mierda = (s2 == H) && 
                    (s1 == O || s1 == N || s1 == C || s1 == P || s1 == B || s1 == Si || s1 == As || s1 == Sb);
                
                int is_aun_mas_mierda = (ch1 > 0 && ch2 > 0) && 
                    (get_electronegativity(c1) <= get_electronegativity(c2));

                if (!is_mierda && !is_aun_mas_mierda) {
                    return -1;
                }
            }
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
    if(has_mult_elements(node, &first_seen) == 0) {
        int obj_charge = node->charge;
        int count_x = 0;
        UnknownX *inc = (UnknownX *)malloc(sizeof(UnknownX)*64);
        pre_process(node, 1, NULL, &obj_charge, &count_x, inc);
        int total_atoms = 0;
        for(int i = 0; i < count_x; i++) {
            total_atoms += inc[i].total_mult;
        }
        int result = -1;
        if(total_atoms > 0 && node->charge % total_atoms == 0) {
            int expected_ox = node->charge / total_atoms;
            for(int i = 0; i < count_x; i++) {
                inc[i].node->charge = expected_ox;
                inc[i].node->charge_done = 1;
            }
            if(check_order(node) != -1) {
                resolve_nodes(node);
                result = 1;
            }
        }
        free(inc);
        return result; // No bt
    } else {
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
    for(int i = 0; i < root->child_count; i++) {
        Node *formula = root->data.child[i];
        for(int j = 0; j < formula->child_count; j++) {
            Node *expression = formula->data.child[j];
            Element *first_seen = NULL;
            if(!(has_mult_elements(expression, &first_seen) == 0)) {
                if(fix_charges(expression) == -1) {
                    fprintf(stderr, "Incorrect charge in formula\n");
                    return -1;
                }
            }
            // Then, calculate final charges
            if(calculate_charges(expression) == -1) {
                fprintf(stderr, "Can't calculate charges\n");
                return -1;
            }
        }
    }
    return 1;
}