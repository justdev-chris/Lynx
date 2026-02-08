#ifndef lynx_h
#define lynx_h

typedef enum {
    TOKEN_ROAR, TOKEN_STALK, TOKEN_SET, TOKEN_POUNCE, TOKEN_CHASE, 
    TOKEN_STALK_PACK, TOKEN_HUNT, TOKEN_HELP, TOKEN_ID, TOKEN_NUMBER, TOKEN_STRING,
    TOKEN_EQUALS, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_GREATER, TOKEN_LESS, TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
} Token;

void initScanner(const char* source);
Token scanToken();
Token peekToken();
void parse_statement();
void runFile(const char* path);

void pounce_store(const char* name, double value);
double pounce_get(const char* name);
void pounce_list(); // For the Hunt command

#endif
