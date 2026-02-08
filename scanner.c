#include <string.h>
#include <ctype.h>
#include "lynx.h"

static const char* start;
static const char* current;

void initScanner(const char* source) {
    start = source;
    current = source;
}

Token makeToken(TokenType type) {
    Token token = {type, start, (int)(current - start)};
    return token;
}

Token scanToken() {
    while (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r') current++;
    start = current;
    if (*current == '\0') return makeToken(TOKEN_EOF);

    char c = *current++;
    
    if (c == '"') { // String detection for paths
        while (*current != '"' && *current != '\0') current++;
        if (*current == '"') current++;
        return makeToken(TOKEN_STRING);
    }
    if (isdigit(c)) {
        while (isdigit(*current)) current++;
        return makeToken(TOKEN_NUMBER);
    }
    if (isalpha(c)) {
        while (isalnum(*current)) current++;
        int len = (int)(current - start);
        if (strncmp(start, "Roar", len) == 0) return makeToken(TOKEN_ROAR);
        if (strncmp(start, "Set", len) == 0) return makeToken(TOKEN_SET);
        if (strncmp(start, "Pounce", len) == 0) return makeToken(TOKEN_POUNCE);
        if (strncmp(start, "Stalk_Pack", len) == 0) return makeToken(TOKEN_STALK_PACK);
        if (strncmp(start, "Stalk", len) == 0) return makeToken(TOKEN_STALK);
        return makeToken(TOKEN_ID);
    }
    if (c == '=') return makeToken(TOKEN_EQUALS);
    if (c == '+') return makeToken(TOKEN_PLUS);
    if (c == '-') return makeToken(TOKEN_MINUS);
    if (c == '*') return makeToken(TOKEN_STAR);
    if (c == '/') return makeToken(TOKEN_SLASH);
    if (c == '>') return makeToken(TOKEN_GREATER);
    if (c == '<') return makeToken(TOKEN_LESS);

    return makeToken(TOKEN_ERROR);
}

Token peekToken() {
    const char* old_start = start;
    const char* old_current = current;
    Token t = scanToken();
    start = old_start;
    current = old_current;
    return t;
}
