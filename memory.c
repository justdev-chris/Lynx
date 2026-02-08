#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

void hunt() {
    printf("\n🐾 DEN CONTENTS (Sorted):\n");
    // Simple Alpha Sort
    for (int i = 0; i < varCount - 1; i++) {
        for (int j = 0; j < varCount - i - 1; j++) {
            if (strcmp(den[j].name, den[j+1].name) > 0) {
                Variable temp = den[j];
                den[j] = den[j+1];
                den[j+1] = temp;
            }
        }
    }
    for (int i = 0; i < varCount; i++) {
        printf("   %-12s : %.5f\n", den[i].name, den[i].value);
    }
    printf("\n");
}
