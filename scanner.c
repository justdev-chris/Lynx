#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lynx.h"
#include "platform.h"

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.col = 1;
    
    // Skip UTF-8 BOM (EF BB BF)
    if ((unsigned char)source[0] == 0xEF && 
        (unsigned char)source[1] == 0xBB && 
        (unsigned char)source[2] == 0xBF) {
        scanner.current += 3;
        scanner.start += 3;
    }
    
    // Skip UTF-16 LE BOM (FF FE)
    if ((unsigned char)source[0] == 0xFF && 
        (unsigned char)source[1] == 0xFE) {
        scanner.current += 2;
        scanner.start += 2;
    }
    
    // Skip UTF-16 BE BOM (FE FF)
    if ((unsigned char)source[0] == 0xFE && 
        (unsigned char)source[1] == 0xFF) {
        scanner.current += 2;
        scanner.start += 2;
    }
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.col++;
    return *scanner.current++;
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    return scanner.current[1];
}

static Token makeToken(LynxTokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    token.col = scanner.col - token.length;
    return token;
}

const char* tokenTypeToString(LynxTokenType type) {
    switch (type) {
        case TOKEN_SET: return "Set";
        case TOKEN_ROAR: return "Roar";
        case TOKEN_HUNT: return "Hunt";
        case TOKEN_STALK_PACK: return "Stalk_Pack";
        case TOKEN_POUNCE: return "Pounce";
        case TOKEN_IF: return "If";
        case TOKEN_ELSE: return "Else";
        case TOKEN_LOAD_LIB: return "LoadLib";
        case TOKEN_FUNC: return "Func";
        case TOKEN_RETURN: return "Return";
        case TOKEN_FOR: return "For";
        case TOKEN_WHILE: return "While";
        case TOKEN_BREAK: return "Break";
        case TOKEN_CONTINUE: return "Continue";
        case TOKEN_AND: return "And";
        case TOKEN_OR: return "Or";
        case TOKEN_NOT: return "Not";
        case TOKEN_RUN: return "Run";
        case TOKEN_GETENV: return "getenv";
        case TOKEN_TRY: return "Try";
        case TOKEN_CATCH: return "Catch";
        case TOKEN_ARGV: return "Argv";
        case TOKEN_EXPORT: return "Export";
        case TOKEN_KITTY_WRITE_FILE: return "KittyWriteFile";
        case TOKEN_KITTY_READ_FILE: return "KittyReadFile";
        case TOKEN_PAW: return "Paw";
        case TOKEN_KITTY_FILE_EXISTS: return "KittyFileExists";
        case TOKEN_KITTY_LIST_FILES: return "KittyListFiles";
        case TOKEN_KITTY_REMOVE_FILE: return "KittyRemoveFile";
        case TOKEN_KITTY_READ_DIR: return "KittyReadDir";
        case TOKEN_KITTY_PORT: return "KittyPort";
        case TOKEN_GET_ERROR: return "GetError";
        case TOKEN_STRING_SPLIT: return "KittySplitString";
        case TOKEN_STRING_CONTAINS: return "KittyCheckIfStringContains";
        case TOKEN_STRING_REPLACE: return "KittyReplaceString";
        case TOKEN_TRIM: return "Trim";
        case TOKEN_LEN: return "Len";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_STRING: return "string literal";
        case TOKEN_NUMBER: return "number";
        case TOKEN_EQUAL: return "=";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_MODULO: return "%";
        case TOKEN_INCREMENT: return "++";
        case TOKEN_DECREMENT: return "--";
        case TOKEN_EQ: return "==";
        case TOKEN_NE: return "!=";
        case TOKEN_GT: return ">";
        case TOKEN_LT: return "<";
        case TOKEN_GE: return ">=";
        case TOKEN_LE: return "<=";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "error token";
        default: return "unknown";
    }
}

char* getTokenText(Token t) {
    static char buffer[256];
    if (t.length < 255) {
        snprintf(buffer, t.length + 1, "%s", t.start);
    } else {
        snprintf(buffer, sizeof(buffer), "%s...", t.start);
    }
    return buffer;
}

static LynxTokenType checkKeyword() {
    const char* s = scanner.start;
    int len = (int)(scanner.current - scanner.start);

    // Direct character comparison – NO string functions
    if (len == 3 && s[0] == 'T' && s[1] == 'r' && s[2] == 'y') return TOKEN_TRY;
    if (len == 5 && s[0] == 'C' && s[1] == 'a' && s[2] == 't' && s[3] == 'c' && s[4] == 'h') return TOKEN_CATCH;
    if (len == 3 && s[0] == 'S' && s[1] == 'e' && s[2] == 't') return TOKEN_SET;
    if (len == 4 && s[0] == 'R' && s[1] == 'o' && s[2] == 'a' && s[3] == 'r') return TOKEN_ROAR;
    if (len == 4 && s[0] == 'H' && s[1] == 'u' && s[2] == 'n' && s[3] == 't') return TOKEN_HUNT;
    if (len == 4 && s[0] == 'H' && s[1] == 'e' && s[2] == 'l' && s[3] == 'p') return TOKEN_HELP;
    if (len == 10 && s[0] == 'S' && s[1] == 't' && s[2] == 'a' && s[3] == 'l' && s[4] == 'k' && s[5] == '_' && s[6] == 'P' && s[7] == 'a' && s[8] == 'c' && s[9] == 'k') return TOKEN_STALK_PACK;
    if (len == 6 && s[0] == 'P' && s[1] == 'o' && s[2] == 'u' && s[3] == 'n' && s[4] == 'c' && s[5] == 'e') return TOKEN_POUNCE;
    if (len == 2 && s[0] == 'I' && s[1] == 'f') return TOKEN_IF;
    if (len == 4 && s[0] == 'E' && s[1] == 'l' && s[2] == 's' && s[3] == 'e') return TOKEN_ELSE;
    if (len == 7 && s[0] == 'L' && s[1] == 'o' && s[2] == 'a' && s[3] == 'd' && s[4] == 'L' && s[5] == 'i' && s[6] == 'b') return TOKEN_LOAD_LIB;
    if (len == 4 && s[0] == 'F' && s[1] == 'u' && s[2] == 'n' && s[3] == 'c') return TOKEN_FUNC;
    if (len == 6 && s[0] == 'R' && s[1] == 'e' && s[2] == 't' && s[3] == 'u' && s[4] == 'r' && s[5] == 'n') return TOKEN_RETURN;
    if (len == 3 && s[0] == 'F' && s[1] == 'o' && s[2] == 'r') return TOKEN_FOR;
    if (len == 5 && s[0] == 'W' && s[1] == 'h' && s[2] == 'i' && s[3] == 'l' && s[4] == 'e') return TOKEN_WHILE;
    if (len == 5 && s[0] == 'B' && s[1] == 'r' && s[2] == 'e' && s[3] == 'a' && s[4] == 'k') return TOKEN_BREAK;
    if (len == 8 && s[0] == 'C' && s[1] == 'o' && s[2] == 'n' && s[3] == 't' && s[4] == 'i' && s[5] == 'n' && s[6] == 'u' && s[7] == 'e') return TOKEN_CONTINUE;
    if (len == 3 && s[0] == 'A' && s[1] == 'n' && s[2] == 'd') return TOKEN_AND;
    if (len == 2 && s[0] == 'O' && s[1] == 'r') return TOKEN_OR;
    if (len == 3 && s[0] == 'N' && s[1] == 'o' && s[2] == 't') return TOKEN_NOT;
    if (len == 3 && s[0] == 'R' && s[1] == 'u' && s[2] == 'n') return TOKEN_RUN;
    if (len == 6 && s[0] == 'g' && s[1] == 'e' && s[2] == 't' && s[3] == 'e' && s[4] == 'n' && s[5] == 'v') return TOKEN_GETENV;
    if (len == 4 && s[0] == 'A' && s[1] == 'r' && s[2] == 'g' && s[3] == 'v') return TOKEN_ARGV;
    if (len == 6 && s[0] == 'E' && s[1] == 'x' && s[2] == 'p' && s[3] == 'o' && s[4] == 'r' && s[5] == 't') return TOKEN_EXPORT;
    if (len == 14 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'W' && s[6] == 'r' && s[7] == 'i' && s[8] == 't' && s[9] == 'e' && s[10] == 'F' && s[11] == 'i' && s[12] == 'l' && s[13] == 'e') return TOKEN_KITTY_WRITE_FILE;
    if (len == 13 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'R' && s[6] == 'e' && s[7] == 'a' && s[8] == 'd' && s[9] == 'F' && s[10] == 'i' && s[11] == 'l' && s[12] == 'e') return TOKEN_KITTY_READ_FILE;
    if (len == 3 && s[0] == 'P' && s[1] == 'a' && s[2] == 'w') return TOKEN_PAW;
    if (len == 15 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'F' && s[6] == 'i' && s[7] == 'l' && s[8] == 'e' && s[9] == 'E' && s[10] == 'x' && s[11] == 'i' && s[12] == 's' && s[13] == 't' && s[14] == 's') return TOKEN_KITTY_FILE_EXISTS;
    if (len == 14 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'L' && s[6] == 'i' && s[7] == 's' && s[8] == 't' && s[9] == 'F' && s[10] == 'i' && s[11] == 'l' && s[12] == 'e' && s[13] == 's') return TOKEN_KITTY_LIST_FILES;
    if (len == 15 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'R' && s[6] == 'e' && s[7] == 'm' && s[8] == 'o' && s[9] == 'v' && s[10] == 'e' && s[11] == 'F' && s[12] == 'i' && s[13] == 'l' && s[14] == 'e') return TOKEN_KITTY_REMOVE_FILE;
    if (len == 12 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'R' && s[6] == 'e' && s[7] == 'a' && s[8] == 'd' && s[9] == 'D' && s[10] == 'i' && s[11] == 'r') return TOKEN_KITTY_READ_DIR;
    if (len == 9 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'P' && s[6] == 'o' && s[7] == 'r' && s[8] == 't') return TOKEN_KITTY_PORT;
    if (len == 8 && s[0] == 'G' && s[1] == 'e' && s[2] == 't' && s[3] == 'E' && s[4] == 'r' && s[5] == 'r' && s[6] == 'o' && s[7] == 'r') return TOKEN_GET_ERROR;
    if (len == 16 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'S' && s[6] == 'p' && s[7] == 'l' && s[8] == 'i' && s[9] == 't' && s[10] == 'S' && s[11] == 't' && s[12] == 'r' && s[13] == 'i' && s[14] == 'n' && s[15] == 'g') return TOKEN_STRING_SPLIT;
    if (len == 26 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'C' && s[6] == 'h' && s[7] == 'e' && s[8] == 'c' && s[9] == 'k' && s[10] == 'I' && s[11] == 'f' && s[12] == 'S' && s[13] == 't' && s[14] == 'r' && s[15] == 'i' && s[16] == 'n' && s[17] == 'g' && s[18] == 'C' && s[19] == 'o' && s[20] == 'n' && s[21] == 't' && s[22] == 'a' && s[23] == 'i' && s[24] == 'n' && s[25] == 's') return TOKEN_STRING_CONTAINS;
    if (len == 18 && s[0] == 'K' && s[1] == 'i' && s[2] == 't' && s[3] == 't' && s[4] == 'y' && s[5] == 'R' && s[6] == 'e' && s[7] == 'p' && s[8] == 'l' && s[9] == 'a' && s[10] == 'c' && s[11] == 'e' && s[12] == 'S' && s[13] == 't' && s[14] == 'r' && s[15] == 'i' && s[16] == 'n' && s[17] == 'g') return TOKEN_STRING_REPLACE;
    if (len == 4 && s[0] == 'T' && s[1] == 'r' && s[2] == 'i' && s[3] == 'm') return TOKEN_TRIM;
    if (len == 3 && s[0] == 'L' && s[1] == 'e' && s[2] == 'n') return TOKEN_LEN;

    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    return makeToken(checkKeyword());
}

static Token number() {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peekNext())) {
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

static void skip_whitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == '\n') {
            scanner.line++;
            scanner.col = 1;
            advance();
        } else if (c == '\r') {
            advance();
            if (peek() == '\n') {
                scanner.line++;
                scanner.col = 1;
                advance();
            }
        } else if (isspace(c)) {
            advance();
        } else {
            break;
        }
    }
}

Token scanToken() {
    skip_whitespace();

    if (peek() == '#') {
        while (peek() != '\n' && !isAtEnd()) advance();
        if (isAtEnd()) return makeToken(TOKEN_EOF);
        return scanToken();
    }

    if (peek() == '/' && peekNext() == '/') {
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
        case '+':
            if (peek() == '+') { advance(); return makeToken(TOKEN_INCREMENT); }
            return makeToken(TOKEN_PLUS);
        case '-':
            if (peek() == '-') { advance(); return makeToken(TOKEN_DECREMENT); }
            return makeToken(TOKEN_MINUS);
        case '*': return makeToken(TOKEN_STAR);
        case '/': return makeToken(TOKEN_SLASH);
        case '%': return makeToken(TOKEN_MODULO);
        case '{': return makeToken(TOKEN_LBRACE);
        case '}': return makeToken(TOKEN_RBRACE);
        case '(': return makeToken(TOKEN_LPAREN);
        case ')': return makeToken(TOKEN_RPAREN);
        case '[': return makeToken(TOKEN_LBRACKET);
        case ']': return makeToken(TOKEN_RBRACKET);
        case ',': return makeToken(TOKEN_COMMA);
        case ':': return makeToken(TOKEN_COLON);
        case ';': return makeToken(TOKEN_SEMICOLON);  // FIXED: Added semicolon support
        case '=':
            if (peek() == '=') { advance(); return makeToken(TOKEN_EQ); }
            return makeToken(TOKEN_EQUAL);
        case '!':
            if (peek() == '=') { advance(); return makeToken(TOKEN_NE); }
            return makeToken(TOKEN_NOT);
        case '>':
            if (peek() == '=') { advance(); return makeToken(TOKEN_GE); }
            return makeToken(TOKEN_GT);
        case '<':
            if (peek() == '=') { advance(); return makeToken(TOKEN_LE); }
            return makeToken(TOKEN_LT);
        case '"': {
            // Fixed: properly handle escape sequences so \" does not terminate the string
            while (!isAtEnd()) {
                char ch = peek();

                if (ch == '"') {
                    // unescaped quote → end of string
                    break;
                }

                if (ch == '\\') {
                    // escape sequence – consume the backslash + the next character
                    advance();               // consume '\'
                    if (isAtEnd()) break;
                    advance();               // consume the escaped character
                    continue;
                }

                if (ch == '\n') {
                    scanner.line++;
                    scanner.col = 1;
                }

                advance();
            }

            if (isAtEnd()) {
                Token t = makeToken(TOKEN_ERROR);
                setErrorF("Unterminated string literal", t.line, t.col);
                return t;
            }

            advance(); // consume the closing "
            return makeToken(TOKEN_STRING);
        }
    }

    Token t = makeToken(TOKEN_ERROR);
    char msg[256];
    snprintf(msg, sizeof(msg), "Unexpected character '%c'", c);
    setErrorF(msg, t.line, t.col);
    return t;
}