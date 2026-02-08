#include <stdlib.h>
#include <string.h>
#include "lynx.h"

typedef struct Variable {
    char* name;
    double value;
    struct Variable* next;
} Variable;

Variable* den = NULL;

void pounce_store(const char* name, double value) {
    Variable* curr = den;
    while (curr) {
        if (strcmp(curr->name, name) == 0) { curr->value = value; return; }
        curr = curr->next;
    }
    Variable* new_v = malloc(sizeof(Variable));
    new_v->name = _strdup(name); // Windows uses _strdup
    new_v->value = value;
    new_v->next = den;
    den = new_v;
}

double pounce_get(const char* name) {
    Variable* curr = den;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr->value;
        curr = curr->next;
    }
    return 0;
}
