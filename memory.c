#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
        if (strcmp(curr->name, name) == 0) {
            curr->value = value;
            return;
        }
        curr = curr->next;
    }
    Variable* new_v = malloc(sizeof(Variable));
    new_v->name = _strdup(name);
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

void pounce_list() {
    Variable* curr = den;
    printf("\n🐾 DEN CONTENTS:\n");
    while (curr) {
        printf("   %-12s : %.5f\n", curr->name, curr->value);
        curr = curr->next;
    }
    printf("\n");
}

void pounce_stash(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    Variable* curr = den;
    while (curr) {
        fprintf(f, "Set %s = %f\n", curr->name, curr->value);
        curr = curr->next;
    }
    fclose(f);
    printf("🐾 Stashed to %s\n", path);
}
