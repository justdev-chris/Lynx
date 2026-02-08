#ifndef lynx_h
#define lynx_h

typedef enum {
    TOKEN_ROAR, TOKEN_STALK, TOKEN_SET, TOKEN_POUNCE,
    TOKEN_ID, TOKEN_NUMBER, TOKEN_EQUALS, TOKEN_PLUS, 
    TOKEN_MINUS, TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
} Token;

// Function Prototypes
void initScanner(const char* source);
Token scanToken();
void pounce_store(const char* name, double value);
double pounce_get(const char* name);
void parse_statement();

#endif
