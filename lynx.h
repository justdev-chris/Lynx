#ifndef LYNX_H
#define LYNX_H

// 1. Token Types
typedef enum {
    TOKEN_SET, TOKEN_ROAR, TOKEN_HUNT, TOKEN_STALK_PACK, TOKEN_HELP,
    TOKEN_POUNCE, TOKEN_IF, TOKEN_ELSE,
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_EQUAL, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_EOF, TOKEN_ERROR
} LynxTokenType;

typedef struct {
    LynxTokenType type;
    const char* start;
    int length;
    int line;
} Token;

// 2. Scanner State Structure
typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

// 3. Global Scanner Declaration
extern Scanner scanner;

// 4. Function Prototypes
void initScanner(const char* source);
Token scanToken();
Token peekToken();

void parse_statement();
void parse_block();
double parse_expression();
int check_condition();
void runFile(const char* path);

void setVar(const char* name, double value);
double getVar(const char* name);
void pounce(const char* name);
void hunt();

#endif