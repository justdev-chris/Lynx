#ifndef LYNX_H
#define LYNX_H

// Renamed to avoid Windows header conflicts
typedef enum {
    TOKEN_SET, TOKEN_ROAR, TOKEN_HUNT, TOKEN_STALK_PACK, TOKEN_HELP,
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_EQUAL, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_EOF, TOKEN_ERROR
} LynxTokenType;

typedef struct {
    LynxTokenType type;
    const char* start;
    int length;
    int line;
} Token;

// Scanner Functions
void initScanner(const char* source);
Token scanToken();
Token peekToken(); // Declared here so main.c can see it

// Parser & Memory Functions
void parse_statement();
void runFile(const char* path);
void setVar(const char* name, double value);
double getVar(const char* name);
void hunt();

#endif
