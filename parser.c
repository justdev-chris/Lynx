#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynx.h"

double get_value() {
    Token t = scanToken();
    if (t.type == TOKEN_NUMBER) {
        return atof(t.start);
    }
    if (t.type == TOKEN_IDENTIFIER) {
        char name[64];
        snprintf(name, t.length + 1, "%s", t.start);
        return getVar(name); // Matches memory.c
    }
    return 0;
}

void parse_statement() {
    Token t = scanToken();

    if (t.type == TOKEN_HUNT) {
        hunt();
        return;
    }

    if (t.type == TOKEN_ROAR) {
        Token val = scanToken();
        if (val.type == TOKEN_STRING) {
            // Print string without quotes
            for(int i = 1; i < val.length - 1; i++) printf("%c", val.start[i]);
            printf("\n");
        } else if (val.type == TOKEN_IDENTIFIER) {
            char name[64];
            snprintf(name, val.length + 1, "%s", val.start);
            printf("%.5f\n", getVar(name));
        }
        return;
    }

    if (t.type == TOKEN_SET) {
        Token nameToken = scanToken();
        char varName[64];
        snprintf(varName, nameToken.length + 1, "%s", nameToken.start);

        scanToken(); // Skip '='
        double val = get_value();
        setVar(varName, val);
        return;
    }

    if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        char path[256];
        snprintf(path, pathToken.length - 1, "%s", pathToken.start + 1);
        runFile(path); // This is now declared in lynx.h
        return;
    }
}
