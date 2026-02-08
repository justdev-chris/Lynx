#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lynx.h"

double get_value(Token t) {
    if (t.type == TOKEN_NUMBER) return atof(t.start);
    if (t.type == TOKEN_ID) {
        char varName[32] = {0};
        strncpy(varName, t.start, t.length);
        return pounce_get(varName);
    }
    return 0;
}

void parse_statement() {
    Token t = scanToken();
    if (t.type == TOKEN_EOF) return;

    if (t.type == TOKEN_ID) {
        if (strncmp(t.start, "Help", t.length) == 0) {
            printf("\n🐾 LYNX COMMANDS:\nSet x = 5 + 2\nRoar x\nHunt\nStash \"file.lnx\"\nStalk_Pack \"file.lnx\"\nHelp\n\n");
            return;
        }
        if (strncmp(t.start, "Hunt", t.length) == 0) { pounce_list(); return; }
        if (strncmp(t.start, "Stash", t.length) == 0) {
            Token pathToken = scanToken();
            char path[256] = {0};
            strncpy(path, pathToken.start + 1, pathToken.length - 2);
            pounce_stash(path);
            return;
        }
    }

    if (t.type == TOKEN_SET) {
        Token name = scanToken();
        scanToken(); // consume '='
        Token left = scanToken();
        double finalVal = get_value(left);

        Token op = peekToken();
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS || op.type == TOKEN_STAR || op.type == TOKEN_SLASH) {
            scanToken(); // consume the operator
            Token right = scanToken();
            double rightVal = get_value(right);
            if (op.type == TOKEN_PLUS) finalVal += rightVal;
            else if (op.type == TOKEN_MINUS) finalVal -= rightVal;
            else if (op.type == TOKEN_STAR) finalVal *= rightVal;
            else if (op.type == TOKEN_SLASH && rightVal != 0) finalVal /= rightVal;
        }

        char varName[32] = {0};
        strncpy(varName, name.start, name.length);
        pounce_store(varName, finalVal);
    } 
    else if (t.type == TOKEN_ROAR) {
        Token name = scanToken();
        char varName[32] = {0};
        strncpy(varName, name.start, name.length);
        double val = pounce_get(varName);

        if ((val >= 30 && val <= 37) || val == 0) {
            printf("\033[%dm", (int)val);
        } else {
            printf("%.5f\n", val);
        }
    }
    else if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        char path[256] = {0};
        if (pathToken.start[0] == '"') {
            strncpy(path, pathToken.start + 1, pathToken.length - 2);
        } else {
            strncpy(path, pathToken.start, pathToken.length);
        }
        runFile(path);
    }
}
