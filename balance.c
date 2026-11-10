#include "balance.h"
#include "parse.h"
#include "table.h"
#include <stdlib.h>
#include <stdio.h>

//helpers
int get_mcd(int x, int y) {
    while(y != 0) {
        int tmp = y;
        y = x % y;
        x = tmp;
    }
    return abs(x);
}

int get_mcm(int x, int y) {
    if(x == 0 || y == 0) return 0;
    return (x/get_mcd(x,y))*y; 
}

Fraction simplify(Fraction f) {
    if (f.num == 0) return (Fraction){ .num = 0, .den = 1 };
    int mcd = get_mcd(f.num, f.den);
    f.num /= mcd;
    f.den /= mcd;
    if(f.den < 0) {
        f.num = -f.num;
        f.den = -f.den;
    }
    return f;
}

Fraction add_fraction(Fraction x, Fraction y) { 
    int mcm = get_mcm(x.den, y.den);
    int num = x.num * (mcm / x.den) + y.num * (mcm / y.den);
    return simplify((Fraction) { .num = num, .den = mcm });
}

Fraction sub_fraction(Fraction x, Fraction y) {
    int mcm = get_mcm(x.den, y.den);
    int num = x.num * (mcm / x.den) - y.num * (mcm / y.den);
    return simplify((Fraction) { .num = num, .den = mcm });
}

Fraction mult_fraction(Fraction x, Fraction y) {
    int num = x.num * y.num;
    int den = x.den * y.den;
    return simplify((Fraction) { .num = num, .den = den });
}

Fraction divide_fraction(Fraction x, Fraction y) {
    if (y.num == 0) {
        fprintf(stderr, "Error: División por cero.\n");
        return (Fraction) { .num = 0, .den = 0 };
    }
    int num = x.num * y.den;
    int den = x.den * y.num;
    return simplify((Fraction) { .num = num, .den = den });
}

Fraction inverse(Fraction x) {
    return simplify((Fraction) { .num = x.den, .den = x.num });
}

int get_expression_count(Node *root) {
    // Root has 2 formulas
    int count = 0;
    for(int i = 0; i < root->child_count; i++) {
        Node *formula = root->data.child[i];
        count += formula->child_count;
    }
    return count;
}

void get_unique_elements(Node *node, int *list, int *count, char *seen) {
    if(node == NULL) return;
    if(node->type == N_ELEMENT) {
        if(!seen[node->data.elem->symbol]) {
            seen[node->data.elem->symbol] = 1;
            list[*count] = node->data.elem->symbol;
            (*count)++;
        }
        return;
    }
    for(int i = 0; i < node->child_count; i++) {
        get_unique_elements(node->data.child[i], list, count, seen);
    }
}

int get_elem_idx(int *list, ElementSymbol symbol, int count) {
    for(int i = 0; i < count; i++) {
        if(list[i] == symbol) return i;
    }
    return -1;
}

void fill_matrix_expr(Fraction **matrix, int sign, int exp_idx, Node *node, int *list, int count, int mult) {
    if(node == NULL) return;
    if(node->type == N_ELEMENT) {
        int elem_idx = get_elem_idx(list, node->data.elem->symbol, count);
        matrix[elem_idx][exp_idx].num += node->mult * sign * mult;
        return;
    }
    int new_mult;
    new_mult = mult*node->mult;
    for(int i = 0; i < node->child_count; i++) {
        fill_matrix_expr(matrix, sign, exp_idx, node->data.child[i], list, count, new_mult);
    }
}

void gauss_jordan(Fraction **matrix, int elem_count, int expr_count, int *coef) {
    int pivot = 0, column = 0;
    while(pivot < elem_count && column < expr_count) {
        int i;
        for(i = pivot; i < elem_count; i++) {
            if(matrix[i][column].num != 0) break;
        }
        if(i == elem_count) {
            column++;
            continue;
        }
        if(i != pivot) {
            Fraction *tmp = matrix[pivot];
            matrix[pivot] = matrix[i];
            matrix[i] = tmp;
        }
        Fraction inv = inverse(matrix[pivot][column]);
        for(i = column; i <= expr_count; i++) {
            matrix[pivot][i] = mult_fraction(matrix[pivot][i], inv);
        }
        for(i = 0; i < elem_count; i++) {
            if(i == pivot) continue;
            Fraction factor = matrix[i][column];
            if(factor.num == 0) continue;
            else {
                for(int j = column; j <= expr_count; j++) {
                    matrix[i][j] = sub_fraction(matrix[i][j], mult_fraction(factor, matrix[pivot][j]));
                }
            }
        }
        pivot++;
        column++;
    }
    //kernel
    Fraction solutions[expr_count];
    for(int i = 0; i < expr_count; i++) {
        solutions[i].den = 1;
        solutions[i].num = 1;
    }
    int j;
    for(int i = 0; i < elem_count; i++) {
        for(j = 0; j < expr_count; j++) {
            if(matrix[i][j].num != 0) break;
        }
        if(j == expr_count) continue;
        solutions[j] = (Fraction) {0,1};
        for(int v = j+1; v < expr_count; v++) {
            solutions[j] = sub_fraction(solutions[j], mult_fraction(matrix[i][v], solutions[v]));
        }
    }
    int mcm = 1;
    for(int i = 0; i < expr_count; i++) {
        mcm = get_mcm(mcm, solutions[i].den);
    }
    for(int i = 0; i < expr_count; i++) {
        solutions[i] = mult_fraction(solutions[i], (Fraction){mcm, 1});
        coef[i] = abs(solutions[i].num);
    }
}

void apply_coefficients(int *solution, Node *root) {
    int current_expr = 0;
    for(int i = 0; i < root->child_count; i++) {
        Node *formula = root->data.child[i];
        for(int j = 0; j < formula->child_count; j++) {  // expr
            formula->data.child[j]->mult = solution[current_expr];
            current_expr++;
        }
    }
}

void balance_equation(Node *root) {
    int element_count = 0;
    // fill matrix
    int list[MAX_VARIABLES];
    char seen[119] = {0};
    get_unique_elements(root, list, &element_count, seen);
    int expression_count = get_expression_count(root);
    Fraction **matrix = (Fraction **)malloc(sizeof(Fraction*) * element_count);
    for(int i = 0; i < element_count; i++) {
        matrix[i] = (Fraction *)malloc((expression_count+1) * sizeof(Fraction));
        for(int j = 0; j <= expression_count; j++) {
            matrix[i][j].num = 0;
            matrix[i][j].den = 1;
        }
    }
    int current_col = 0;
    for(int i = 0; i < root->child_count; i++) {
        Node *formula = root->data.child[i];
        for(int j = 0; j < formula->child_count; j++) {
            fill_matrix_expr(matrix, 1-2*i, current_col, formula->data.child[j], list, element_count, 1);
            current_col++;
        }
    }
    // gauss-jordan
    int solution[expression_count];
    gauss_jordan(matrix, element_count, expression_count, solution);
    // Assign
    apply_coefficients(solution, root);
}