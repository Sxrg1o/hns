#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parse.h"
#include "table.h"

void print_node(Node *node, int level) {
    char space[128] = "";
    for(int i=0; i < level && i < 16; i++) { // cap for now
        strcat(space, "        ");
    }
    printf("%s=========\n", space);
    if(node->type == N_ELEMENT) {
        printf("%sElem: %s\n", space, symbol_to_string(node->data.elem->symbol));
    } else {
        printf("%s", space);
        if(node->type == N_GROUP) printf("Group Node\n");
        if(node->type == N_COMPOUND) printf("Comp Node\n");
        if(node->type == N_ADDUCT) printf("Aduc Node\n");
    }
    printf("%sCharge: %d\n", space, node->charge);
    printf("%sDone? %d\n", space, node->charge_done);
    printf("%sMult: %d\n", space, node->mult);
    printf("%s=========\n", space);
}

void print_tree(Node *root, int level) {
    if(root == NULL) return;
    print_node(root, level);
    if(root->type == N_ELEMENT) return;
    for(int i=0; i<root->child_count; i++) {
        print_tree(root->data.child[i], level+1);
    }
}

void free_tree(Node *node) {
    if(node == NULL) return;
    if(node->type != N_ELEMENT && node->data.child != NULL) {
        for(int i=0; i < node->child_count; i++) {
            free_tree(node->data.child[i]);
        }
        free(node->data.child);
    }
    free(node);
}

void node_add_child(Node *parent, Node *child) {
    if(parent == NULL || child == NULL) return;

    Node **tmp = (Node **)realloc(parent->data.child, (parent->child_count+1) * sizeof(Node *));
    if(tmp == NULL) {
        fprintf(stderr, "Error: realloc failed in node_add_child\n");
        return;
    }

    parent->data.child = tmp;
    parent->data.child[parent->child_count++] = child;
}

Node* create_group_node(NodeType type) {
    Node *node = (Node *)malloc(sizeof(Node));
    if(node == NULL) {
        fprintf(stderr, "Error: malloc failed in create_group_node\n");
        return NULL;
    }
    node->type = type;
    node->child_count = 0;
    node->mult = 1;
    node->charge = 0;
    node->data.child = NULL;
    node->charge_done = 0;
    return node;
}

Node* create_element_node(Element *element) {
    Node *node = (Node *)malloc(sizeof(Node));
    if (node == NULL) {
        fprintf(stderr, "Error: malloc failed in create_element_node\n");
        return NULL;
    }
    node->type = N_ELEMENT;
    node->child_count = 0;
    node->mult = 1;
    node->charge = 0;
    node->data.elem = element;
    node->charge_done = 0;
    return node;
}

int parse_charge(Token token_list[], int* index, int* output_charge) {
    (*index)++;
    int charge = 1;
    if(token_list[*index].type == T_NUMBER) {
        charge = token_list[*index].value.number;
        (*index)++;
    }
    if(token_list[*index].type == T_SIGN) {
        char signo = token_list[*index].value.sign;
        if(signo == '-') charge *= -1;
        *output_charge = charge;
        (*index)++;
        return 1;
    }
    
    return 0;
}

Node *parse_group(Token token_list[], int* index) {
    (*index)++;
    Node *compound = parse_compound(token_list, index);
    if(compound == NULL) return NULL;
    if(token_list[*index].type == T_CPAREN) {
        (*index)++;
    } else {
        free_tree(compound);
        return NULL;
    }
    compound->type = N_GROUP;
    if(compound->child_count == 1) {
        Node *child = compound->data.child[0];
        if(child->charge_done == 0 && child->mult == 1) {
            free(compound->data.child);
            free(compound);
            return child;
        }
    }
    return compound;
}

Node *parse_core(Token token_list[], int* index) {
    Node *node;
    if(token_list[*index].type == T_ELEMENT) {
        node = create_element_node(token_list[*index].value.elem);
        (*index)++;
    } else if(token_list[*index].type == T_OPAREN) {
        node = parse_group(token_list, index);
        if(node == NULL) return NULL;
    } else {
        return NULL;
    }
    return node;
}

Node *parse_unit(Token token_list[], int* index) {
    Node *core = parse_core(token_list, index);
    if(core == NULL) return NULL;
    if(token_list[*index].type == T_NUMBER) {
        core->mult = token_list[*index].value.number;
        (*index)++;
    }
    if(token_list[*index].type == T_CHARGE) {
        if(!parse_charge(token_list, index, &(core->charge))) {
            free_tree(core);
            return NULL;
        }
        core->charge_done = 1;
    }
    return core;
}

Node *parse_compound(Token token_list[], int* index) {
    Node *parent = create_group_node(N_COMPOUND);
    if(parent == NULL) return NULL;

    Node *first_unit = parse_unit(token_list, index);
    if(first_unit == NULL) {
        free_tree(parent);
        return NULL;
    }
    node_add_child(parent, first_unit);
    while(token_list[*index].type == T_ELEMENT || token_list[*index].type == T_OPAREN) {
        Node *unit = parse_unit(token_list, index);
        if(unit == NULL) {
            free_tree(parent);
            return NULL;
        }
        node_add_child(parent, unit);
    }
    return parent;
}

Node* parse_adduct(Token token_list[], int* index) {
    (*index)++;
    int prefix = 1;
    if(token_list[*index].type == T_NUMBER) {
        prefix = token_list[*index].value.number;
        (*index)++;
    }
    Node *compound = parse_compound(token_list, index);
    if(compound == NULL) return NULL;
    compound->mult = prefix;
    return compound;
}

Node* parse_expression(Token token_list[], int* index) {
    int prefix = 1;
    if(token_list[*index].type == T_NUMBER) {
        prefix = token_list[*index].value.number;
        (*index)++;
    }

    Node *compound = parse_compound(token_list, index);
    if(compound == NULL) return NULL;
    compound->mult = prefix;

    if(token_list[*index].type == T_CHARGE) {
        if(!parse_charge(token_list, index, &(compound->charge))) {
            free_tree(compound);
            return NULL;
        }
        compound->charge_done = 1;
        return compound;
    }
    if(token_list[*index].type == T_MULTIP) {
        Node* parent = create_group_node(N_ADDUCT);
        if(parent == NULL) {
            free_tree(compound);
            return NULL;
        }
        node_add_child(parent, compound);
        Node* first_child = parse_adduct(token_list, index);
        if(first_child == NULL) {
            free_tree(parent);
            return NULL;
        }
        node_add_child(parent, first_child);

        while(token_list[*index].type == T_MULTIP) {
            Node* child = parse_adduct(token_list, index);
            if(child == NULL) {
                free_tree(parent);
                return NULL;
            }
            node_add_child(parent, child);
        }
        return parent;
    }
    return compound;
}

Node* parse_formula(Token token_list[], int* index) {
    Node *expression = parse_expression(token_list, index);
    if(expression == NULL) return NULL;
    if(token_list[*index].type == T_END) return expression;
    free_tree(expression);
    return NULL;
}