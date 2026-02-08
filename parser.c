#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynx.h"

// Helper to get values (Supports Numbers or Variables from the Den)
double get_value() {
    Token t = scanToken();
    if (t.type == TOKEN_NUMBER) {
        return atof(t.start);
    }
    if (t.type == TOKEN_IDENTIFIER) {
        char name[64];
        snprintf(name, t.length + 1, "%s", t.start);
        return getVar(name); 
    }
    return 0;
}

void parse_statement() {
    Token t = scanToken();

    // 🐾 HUNT: Show all variables
    if (t.type == TOKEN_HUNT) {
        hunt();
        return;
    }

    // 🐾 ROAR: Print logic
    if (t.type == TOKEN_ROAR) {
        Token val = scanToken();
        if (val.type == TOKEN_STRING) {
            // Strip quotes and print
            for(int i = 1; i < val.length - 1; i++) printf("%c", val.start[i]);
            printf("\n");
        } else if (val.type == TOKEN_IDENTIFIER) {
            char name[64];
            snprintf(name, val.length + 1, "%s", val.start);
            printf("%.5f\n", getVar(name));
        } else if (val.type == TOKEN_NUMBER) {
            printf("%.5f\n", atof(val.start));
        }
        return;
    }

    // 🐾 SET: Assignment (Set x = 10)
    if (t.type == TOKEN_SET) {
        Token nameToken = scanToken();
        if (nameToken.type != TOKEN_IDENTIFIER) return;

        char varName[64];
        snprintf(varName, nameToken.length + 1, "%s", nameToken.start);

        Token op = scanToken(); // Consumes '='
        if (op.type == TOKEN_EQUAL) {
            double finalVal = get_value();
            setVar(varName, finalVal);
        }
        return;
    }

    // 🐾 STALK_PACK: Import logic
    if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        if (pathToken.type == TOKEN_STRING) {
            char path[256];
            // Remove quotes for the file system
            snprintf(path, pathToken.length - 1, "%s", pathToken.start + 1);
            runFile(path); 
        }
        return;
    }

    // 🐾 HELP / EXIT: Handled primarily in main.c REPL loop
    if (t.type == TOKEN_HELP) return;
}
