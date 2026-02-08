#include <string.h>
#include <ctype.h>
#include "lynx.h" // This MUST be at the top

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

// FIX: Use LynxTokenType here
static Token makeToken(LynxTokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    // token.line = scanner.line; // Add this back if you have 'line' in your Token struct
    return token;
}

// FIX: Use LynxTokenType here
static LynxTokenType checkKeyword() {
    int len = (int)(scanner.current - scanner.start);
    const char* s = scanner.start;
    
    if (len == 3 && strncmp(s, "Set", 3) == 0) return TOKEN_SET;
    if (len == 4 && strncmp(s, "Roar", 4) == 0) return TOKEN_ROAR;
    if (len == 4 && strncmp(s, "Hunt", 4) == 0) return TOKEN_HUNT;
    if (len == 5 && strncmp(s, "Stash", 5) == 0) return TOKEN_STASH;
    if (len == 10 && strncmp(s, "Stalk_Pack", 10) == 0) return TOKEN_STALK_PACK;
    
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
    // Skip whitespace
    while (isspace(peek())) {
        if (advance() == '\n') scanner.line++;
    }

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
            if (!isAtEnd()) advance(); // consume closing quote
            return makeToken(TOKEN_STRING);
        }
    }

    return makeToken(TOKEN_ERROR);
}
