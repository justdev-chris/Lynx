#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynx.h"

typedef struct {
    char name[64];
    double value;
} Variable;

Variable den[100];
int varCount = 0;

void setVar(const char* name, double val) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(den[i].name, name) == 0) {
            den[i].value = val;
            return;
        }
    }
    strcpy(den[varCount].name, name);
    den[varCount++].value = val;
}

double getVar(const char* name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(den[i].name, name) == 0) return den[i].value;
    }
    return 0;
}

void pounce(const char* name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(den[i].name, name) == 0) {
            for (int j = i; j < varCount - 1; j++) {
                den[j] = den[j + 1];
            }
            varCount--;
            printf("🐾 Pounced %s\n", name);
            return;
        }
    }
    printf("🐾 %s not found in den\n", name);
}

void hunt() {
    printf("\n🐾 DEN CONTENTS:\n");
    for (int i = 0; i < varCount; i++) {
        printf("   %-12s : %.5f\n", den[i].name, den[i].value);
    }
}