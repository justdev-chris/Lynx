#ifndef lynx_h
#define lynx_h

// Renamed to LynxTokenType to avoid Windows.h conflicts
typedef enum {
    TOKEN_SET, TOKEN_ROAR, TOKEN_HUNT, TOKEN_STASH, 
    TOKEN_STALK_PACK, TOKEN_HELP, TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, 
    TOKEN_SLASH, TOKEN_EQUAL, TOKEN_STRING, TOKEN_EOF, TOKEN_ERROR
} LynxTokenType;

typedef struct {
    LynxTokenType type; // Use the new enum name here
    const char* start;
    int length;
    int line;
} Token;

// Memory / Den Functions
void setVar(const char* name, double val);
double getVar(const char* name);
void hunt();

// Scanner & Parser Functions
void initScanner(const char* source);
Token scanToken();
Token peekToken();
void parse_statement();
void runFile(const char* path);

#endif
