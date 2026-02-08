#include <string.h>
#include <ctype.h>
#include "lynx.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAtEnd() { return *scanner.current == '\0'; }
static char advance() { return *scanner.current++; }
static char peek() { return *scanner.current; }

static Token makeToken(TokenType type) {
    Token token = {type, scanner.start, (int)(scanner.current - scanner.start), scanner.line};
    return token;
}

static TokenType checkKeyword() {
    int len = (int)(scanner.current - scanner.start);
    const char* s = scanner.start;
    if (strncmp(s, "Set", len) == 0) return TOKEN_SET;
    if (strncmp(s, "Roar", len) == 0) return TOKEN_ROAR;
    if (strncmp(s, "Hunt", len) == 0) return TOKEN_HUNT;
    if (strncmp(s, "Stash", len) == 0) return TOKEN_STASH;
    if (strncmp(s, "Stalk_Pack", len) == 0) return TOKEN_STALK_PACK;
    if (strncmp(s, "Help", len) == 0) return TOKEN_HELP;
    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    return makeToken(checkKeyword());
}

static Token number() {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(scanner.current[1])) {
        advance();
        while (isdigit(peek())) advance();
    }
    return makeToken(TOKEN_NUMBER);
}

Token scanToken() {
    while (isspace(peek())) { if (advance() == '\n') scanner.line++; }
    scanner.start = scanner.current;
    if (isAtEnd()) return makeToken(TOKEN_EOF);
    char c = advance();
    if (isalpha(c) || c == '_') return identifier();
    if (isdigit(c)) return number();
    switch (c) {
        case '+': return makeToken(TOKEN_PLUS);
        case '-': return makeToken(TOKEN_MINUS);
        case '*': return makeToken(TOKEN_STAR);
        case '/': return makeToken(TOKEN_SLASH);
        case '=': return makeToken(TOKEN_EQUAL);
        case '"': {
            while (peek() != '"' && !isAtEnd()) advance();
            advance(); return makeToken(TOKEN_STRING);
        }
    }
    return makeToken(TOKEN_ERROR);
}
