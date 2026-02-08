#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lynx.h"

void display_help() {
    printf("\n🐾 LYNX HELP MENU\n");
    printf("Set [var] = [val]  : Store data\n");
    printf("Roar [var]        : Print data or set color\n");
    printf("Hunt              : Show all variables\n");
    printf("Stalk [var]       : Input number\n");
    printf("Stalk_Pack [file] : Import module\n");
    printf("Help              : Show this menu\n\n");
}

void parse_statement() {
    Token t = scanToken();
    if (t.type == TOKEN_EOF) return;

    // Handle keywords based on token length/content
    if (t.type == TOKEN_ID) {
        if (strncmp(t.start, "Help", t.length) == 0) { display_help(); return; }
        if (strncmp(t.start, "Hunt", t.length) == 0) { pounce_list(); return; }
    }

    if (t.type == TOKEN_SET) {
        Token name = scanToken();
        scanToken(); // '='
        Token val = scanToken();
        char varName[32] = {0};
        strncpy(varName, name.start, name.length);
        pounce_store(varName, atof(val.start));
    } 
    else if (t.type == TOKEN_ROAR) {
        Token name = scanToken();
        char varName[32] = {0};
        strncpy(varName, name.start, name.length);
        double val = pounce_get(varName);

        if ((val >= 30 && val <= 37) || val == 0) {
            printf("\033[%dm", (int)val);
        } else {
            printf("%.2f\n", val);
        }
    }
    else if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        char path[256] = {0};
        // Clean quotes if present
        if (pathToken.start[0] == '"') {
            strncpy(path, pathToken.start + 1, pathToken.length - 2);
        } else {
            strncpy(path, pathToken.start, pathToken.length);
        }
        runFile(path);
    }
}
