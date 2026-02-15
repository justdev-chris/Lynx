#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lynx.h"

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAtEnd() { 
    return *scanner.current == '\0'; 
}

static char advance() { 
    return *scanner.current++; 
}

static char peek() { 
    return *scanner.current; 
}

static Token makeToken(LynxTokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static LynxTokenType checkKeyword() {
    int len = (int)(scanner.current - scanner.start);
    const char* s = scanner.start;
    
    if (len == 3 && strncmp(s, "Set", 3) == 0) return TOKEN_SET;
    if (len == 4 && strncmp(s, "Roar", 4) == 0) return TOKEN_ROAR;
    if (len == 4 && strncmp(s, "Hunt", 4) == 0) return TOKEN_HUNT;
    if (len == 4 && strncmp(s, "Help", 4) == 0) return TOKEN_HELP;
    if (len == 10 && strncmp(s, "Stalk_Pack", 10) == 0) return TOKEN_STALK_PACK;
    if (len == 6 && strncmp(s, "Pounce", 6) == 0) return TOKEN_POUNCE;
    if (len == 2 && strncmp(s, "If", 2) == 0) return TOKEN_IF;
    if (len == 4 && strncmp(s, "Else", 4) == 0) return TOKEN_ELSE;
    if (len == 7 && strncmp(s, "LoadLib", 7) == 0) return TOKEN_LOAD_LIB;
    
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

Token peekToken() {
    Scanner checkpoint = scanner;
    Token token = scanToken();
    scanner = checkpoint;
    return token;
}

Token scanToken() {
    // Skip whitespace
    while (isspace(peek())) {
        if (advance() == '\n') scanner.line++;
    }

    // Handle comments
    if (peek() == '#') {
        while (peek() != '\n' && !isAtEnd()) advance();
        if (isAtEnd()) return makeToken(TOKEN_EOF);
        return scanToken();
    }
    
    if (peek() == '/' && scanner.current[1] == '/') {
        advance(); advance();
        while (peek() != '\n' && !isAtEnd()) advance();
        if (isAtEnd()) return makeToken(TOKEN_EOF);
        return scanToken();
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
        case '{': return makeToken(TOKEN_LBRACE);
        case '}': return makeToken(TOKEN_RBRACE);
        case '=': 
            if (peek() == '=') {
                advance();
                return makeToken(TOKEN_EQ);
            }
            return makeToken(TOKEN_EQUAL);
        case '!':
            if (peek() == '=') {
                advance();
                return makeToken(TOKEN_NE);
            }
            return makeToken(TOKEN_ERROR);
        case '>':
            if (peek() == '=') {
                advance();
                return makeToken(TOKEN_GE);
            }
            return makeToken(TOKEN_GT);
        case '<':
            if (peek() == '=') {
                advance();
                return makeToken(TOKEN_LE);
            }
            return makeToken(TOKEN_LT);
        case '"': {
            while (peek() != '"' && !isAtEnd()) {
                if (peek() == '\n') scanner.line++;
                advance();
            }
            if (isAtEnd()) return makeToken(TOKEN_ERROR);
            advance();
            return makeToken(TOKEN_STRING);
        }
    }

    return makeToken(TOKEN_ERROR);
}