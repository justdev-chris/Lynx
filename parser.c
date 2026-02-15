#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynx.h"

double parse_expression() {
    Token t = scanToken();
    double value;
    
    if (t.type == TOKEN_NUMBER) {
        value = atof(t.start);
    } else if (t.type == TOKEN_IDENTIFIER) {
        char name[64];
        snprintf(name, t.length + 1, "%s", t.start);
        value = getVar(name);
    } else {
        return 0;
    }
    
    Token op = peekToken();
    while (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS || 
           op.type == TOKEN_STAR || op.type == TOKEN_SLASH) {
        scanToken();
        
        Token rightToken = scanToken();
        double right;
        if (rightToken.type == TOKEN_NUMBER) {
            right = atof(rightToken.start);
        } else if (rightToken.type == TOKEN_IDENTIFIER) {
            char name[64];
            snprintf(name, rightToken.length + 1, "%s", rightToken.start);
            right = getVar(name);
        } else {
            break;
        }
        
        switch (op.type) {
            case TOKEN_PLUS:  value += right; break;
            case TOKEN_MINUS: value -= right; break;
            case TOKEN_STAR:  value *= right; break;
            case TOKEN_SLASH: 
                if (right == 0) {
                    printf("🐾 Can't divide by zero!\n");
                    return 0;
                }
                value /= right; 
                break;
        }
        
        op = peekToken();
    }
    
    return value;
}

int check_condition() {
    double left = parse_expression();
    
    Token op = scanToken();
    double right = parse_expression();
    
    switch (op.type) {
        case TOKEN_EQ: return left == right;
        case TOKEN_NE: return left != right;
        case TOKEN_GT: return left > right;
        case TOKEN_LT: return left < right;
        case TOKEN_GE: return left >= right;
        case TOKEN_LE: return left <= right;
        default: 
            printf("🐾 Expected comparison operator\n");
            return 0;
    }
}

void parse_block() {
    Token brace = scanToken();
    if (brace.type != TOKEN_LBRACE) return;
    
    while (peekToken().type != TOKEN_RBRACE && peekToken().type != TOKEN_EOF) {
        parse_statement();
    }
    scanToken();
}

void parse_statement() {
    Token t = scanToken();

    if (t.type == TOKEN_HUNT) {
        hunt();
        return;
    }

    if (t.type == TOKEN_ROAR) {
        Token val = scanToken();
        if (val.type == TOKEN_STRING) {
            for(int i = 1; i < val.length - 1; i++) printf("%c", val.start[i]);
            printf("\n");
        } else if (val.type == TOKEN_IDENTIFIER) {
            char name[64];
            snprintf(name, val.length + 1, "%s", val.start);
            printf("%.5f\n", getVar(name));
        } else if (val.type == TOKEN_NUMBER) {
            printf("%.5f\n", atof(val.start));
        }
        return;
    }

    if (t.type == TOKEN_SET) {
        Token nameToken = scanToken();
        if (nameToken.type != TOKEN_IDENTIFIER) return;

        char varName[64];
        snprintf(varName, nameToken.length + 1, "%s", nameToken.start);

        Token op = scanToken();
        if (op.type == TOKEN_EQUAL) {
            double finalVal = parse_expression();
            setVar(varName, finalVal);
        }
        return;
    }

    if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        if (pathToken.type == TOKEN_STRING) {
            char path[256];
            snprintf(path, pathToken.length - 1, "%s", pathToken.start + 1);
            runFile(path); 
        }
        return;
    }

    if (t.type == TOKEN_POUNCE) {
        Token nameToken = scanToken();
        if (nameToken.type == TOKEN_IDENTIFIER) {
            char varName[64];
            snprintf(varName, nameToken.length + 1, "%s", nameToken.start);
            pounce(varName);
        }
        return;
    }

    if (t.type == TOKEN_IF) {
        int condition = check_condition();
        
        if (condition) {
            parse_block();
        } else {
            // Skip the if block
            Scanner save = scanner;
            parse_block();
            scanner = save;
            
            if (peekToken().type == TOKEN_ELSE) {
                scanToken();
                parse_block();
            }
        }
        return;
    }

    if (t.type == TOKEN_HELP || t.type == TOKEN_EOF) return;
}