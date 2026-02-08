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
    Token token;
    token.type = type;
    token.start = start;
    token.length = (int)(current - start);
    return token;
}

Token scanToken() {
    while (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r') current++;
    start = current;
    if (*current == '\0') return makeToken(TOKEN_EOF);

    char c = *current++;
    if (isdigit(c)) {
        while (isdigit(*current)) current++;
        return makeToken(TOKEN_NUMBER);
    }
    if (isalpha(c)) {
        while (isalnum(*current)) current++;
        int len = (int)(current - start);
        if (strncmp(start, "Roar", len) == 0) return makeToken(TOKEN_ROAR);
        if (strncmp(start, "Set", len) == 0) return makeToken(TOKEN_SET);
        return makeToken(TOKEN_ID);
    }
    if (c == '=') return makeToken(TOKEN_EQUALS);
    if (c == '+') return makeToken(TOKEN_PLUS);
    if (c == '-') return makeToken(TOKEN_MINUS);

    return makeToken(TOKEN_ERROR);
}
