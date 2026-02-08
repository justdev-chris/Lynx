#ifndef lynx_h
#define lynx_h

typedef enum {
    TOKEN_SET, TOKEN_ROAR, TOKEN_HUNT, TOKEN_STASH, 
    TOKEN_STALK_PACK, TOKEN_HELP, TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, 
    TOKEN_SLASH, TOKEN_EQUAL, TOKEN_STRING, TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();
Token peekToken();
void parse_statement();

#endif
