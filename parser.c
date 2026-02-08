#include <stdlib.h>
#include <stdio.h>
#include "lynx.h"

void parse_statement() {
    Token t = scanToken();
    if (t.type == TOKEN_SET) {
        Token name = scanToken();
        scanToken(); // Skip '='
        Token val = scanToken();
        char varName[32] = {0};
        snprintf(varName, name.length + 1, "%s", name.start);
        pounce_store(varName, atof(val.start));
    } else if (t.type == TOKEN_ROAR) {
        Token name = scanToken();
        char varName[32] = {0};
        snprintf(varName, name.length + 1, "%s", name.start);
        printf("🐾 %.2f\n", pounce_get(varName));
    }
}
