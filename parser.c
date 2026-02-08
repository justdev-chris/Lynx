#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lynx.h"

void parse_statement() {
    Token t = scanToken();
    if (t.type == TOKEN_EOF) return;

    if (t.type == TOKEN_SET) {
        Token name = scanToken();
        scanToken(); // skip '='
        Token val = scanToken();
        char varName[32] = {0};
        strncpy(varName, name.start, name.length);
        pounce_store(varName, atof(val.start));
    } 
    else if (t.type == TOKEN_ROAR) {
        Token name = scanToken();
        char varName[32] = {0};
        strncpy(varName, name.start, name.length);
        printf("%.2f\n", pounce_get(varName));
    }
    else if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        char path[256] = {0};
        // Clean up quotes from "file.lnx"
        strncpy(path, pathToken.start + 1, pathToken.length - 2);
        runFile(path);
    }
}
