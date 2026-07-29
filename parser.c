#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <setjmp.h>
#include "lynx.h"
#include "platform.h"

// ─── GLOBALS ──────────────────────────────────────────────────
extern char* lynx_error;
extern LynxError lynx_error_state;
extern TryState try_state;

char* loaded_packages[64];
int loaded_pkg_count = 0;

#define STR_REPLACE_BUFFER_SIZE 65536
static char str_replace_buffer[STR_REPLACE_BUFFER_SIZE];

// ─── SAFE TOKEN TO STRING HELPER ───────────────────────────────
static void safe_token_to_string(Token t, char* out, size_t outlen) {
    if (!out || outlen < 1) return;
    if (!t.start) {
        out[0] = '\0';
        return;
    }
    int len = t.length > (int)outlen - 1 ? (int)outlen - 1 : t.length;
    if (len > 0) {
        strncpy(out, t.start, len);
    }
    out[len] = '\0';
}

// ─── STRING HELPERS ────────────────────────────────────────────
static char* str_trim_copy(const char* str) {
    if (!str) return strdup("");
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return strdup("");
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    size_t len = end - str + 1;
    char* result = malloc(len + 1);
    if (!result) return strdup("");
    strncpy(result, str, len);
    result[len] = '\0';
    return result;
}

static int str_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL;
}

static char* str_replace(const char* src, const char* old, const char* new) {
    if (!src || !old || !new) return str_replace_buffer;
    if (strlen(old) == 0) {
        strncpy(str_replace_buffer, src, STR_REPLACE_BUFFER_SIZE - 1);
        str_replace_buffer[STR_REPLACE_BUFFER_SIZE - 1] = '\0';
        return str_replace_buffer;
    }
    
    char* p = str_replace_buffer;
    const char* q = src;
    size_t old_len = strlen(old);
    size_t new_len = strlen(new);
    size_t remaining = STR_REPLACE_BUFFER_SIZE - 1;
    
    while (*q && remaining > 0) {
        if (strncmp(q, old, old_len) == 0) {
            if (new_len > remaining) {
                fprintf(stderr, "🐾 ERROR: String replacement result too long\n");
                *p = '\0';
                return str_replace_buffer;
            }
            memcpy(p, new, new_len);
            p += new_len;
            remaining -= new_len;
            q += old_len;
        } else {
            *p++ = *q++;
            remaining--;
        }
    }
    *p = '\0';
    return str_replace_buffer;
}

#define MAX_SPLIT_TOKENS 256
static char** split_string(const char* str, const char* delim, int* count) {
    if (!str || !delim || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    char* copy = strdup(str);
    if (!copy) {
        *count = 0;
        return NULL;
    }
    
    char** result = malloc(MAX_SPLIT_TOKENS * sizeof(char*));
    if (!result) {
        free(copy);
        *count = 0;
        return NULL;
    }
    
    *count = 0;
    char* next_token = NULL;
    char* token = strtok_r(copy, delim, &next_token);
    
    while (token && *count < MAX_SPLIT_TOKENS) {
        result[(*count)] = strdup(token);
        if (!result[(*count)]) {
            fprintf(stderr, "🐾 ERROR: Out of memory in split_string\n");
            break;
        }
        (*count)++;
        token = strtok_r(NULL, delim, &next_token);
    }
    
    if (*count >= MAX_SPLIT_TOKENS && token) {
        fprintf(stderr, "🐾 WARNING: Split string limited to %d tokens\n", MAX_SPLIT_TOKENS);
    }
    
    free(copy);
    return result;
}

// ─── ERROR HANDLING ──────────────────────────────────────────────
void clearError() {
    if (lynx_error) {
        free(lynx_error);
        lynx_error = NULL;
    }
    if (lynx_error_state.message) {
        free(lynx_error_state.message);
        lynx_error_state.message = NULL;
    }
    lynx_error_state.line = 0;
    lynx_error_state.col = 0;
}

void setError(const char* msg, int line, int col) {
    if (!msg) msg = "Unknown error";
    
    clearError();
    lynx_error = malloc(512);
    lynx_error_state.message = malloc(512);
    
    if (!lynx_error || !lynx_error_state.message) {
        fprintf(stderr, "🐾 CRITICAL: Out of memory for error handling\n");
        if (lynx_error) free(lynx_error);
        if (lynx_error_state.message) free(lynx_error_state.message);
        lynx_error = NULL;
        lynx_error_state.message = NULL;
        return;
    }
    
    snprintf(lynx_error, 512, "[Line %d, Col %d] %s", line, col, msg);
    strncpy(lynx_error_state.message, lynx_error, 511);
    lynx_error_state.message[511] = '\0';
    lynx_error_state.line = line;
    lynx_error_state.col = col;
    
    if (try_state.is_trying) {
        try_state.error_message = lynx_error_state.message;
        try_state.error_line = line;
        try_state.error_col = col;
        longjmp(try_state.env, 1);
    }
}

void setErrorF(const char* format, ...) {
    if (!format) format = "Unknown error";
    
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    int line = 1, col = 1;
    if (scanner.current != NULL) {
        Token t = peekToken();
        line = t.line;
        col = t.col;
    }
    setError(buffer, line, col);
}

// ─── PARSING ──────────────────────────────────────────────────────

double parse_primary() {
    Token t = scanToken();
    if (t.type == TOKEN_NUMBER) return atof(t.start);
    
    // ─── LEN() ──────────────────────────────────────────────────
    if (t.type == TOKEN_LEN) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("Len expects '('");
            return 0;
        }
        
        Token arg = scanToken();
        if (arg.type == TOKEN_IDENTIFIER) {
            char name[64];
            safe_token_to_string(arg, name, sizeof(name));
            char* str = getVarString(name);
            double result = (double)strlen(str);
            setVar("__result", result);
            
            Token rparen = scanToken();
            if (rparen.type != TOKEN_RPAREN) {
                setErrorF("Len expects ')'");
                return 0;
            }
            return result;
        } else if (arg.type == TOKEN_STRING) {
            char str[4096];
            int strLen = arg.length > 4095 ? 4094 : arg.length - 2;
            if (strLen > 0) {
                strncpy(str, arg.start + 1, strLen);
                str[strLen] = '\0';
            } else {
                str[0] = '\0';
            }
            double result = (double)strlen(str);
            setVar("__result", result);
            
            Token rparen = scanToken();
            if (rparen.type != TOKEN_RPAREN) {
                setErrorF("Len expects ')'");
                return 0;
            }
            return result;
        } else {
            setErrorF("Len expects a string or variable name");
            return 0;
        }
    }
    
    // ─── getenv() ──────────────────────────────────────────────
    if (t.type == TOKEN_GETENV) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("getenv expects '('");
            return 0;
        }
        
        Token arg = scanToken();
        if (arg.type != TOKEN_STRING) {
            setErrorF("getenv expects a string");
            return 0;
        }
        
        char name[256];
        int nameLen = arg.length > 255 ? 254 : arg.length - 2;
        if (nameLen > 0) {
            strncpy(name, arg.start + 1, nameLen);
            name[nameLen] = '\0';
        } else {
            name[0] = '\0';
        }
        
        const char* value = getenv(name);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("getenv expects ')'");
            return 0;
        }
        
        if (value) {
            setVarString("__result", value);
            return 0;
        } else {
            setVarString("__result", "");
            return 0;
        }
    }
    
    // ─── KittyCheckIfStringContains() ──────────────────────────
    if (t.type == TOKEN_STRING_CONTAINS) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("KittyCheckIfStringContains expects '('");
            return 0;
        }
        
        Token hayTok = scanToken();
        Token comma = scanToken();
        Token needleTok = scanToken();
        
        if (hayTok.type != TOKEN_STRING && hayTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyCheckIfStringContains expects string or variable");
            return 0;
        }
        if (needleTok.type != TOKEN_STRING && needleTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyCheckIfStringContains expects string or variable");
            return 0;
        }
        
        char hay[4096], needle[4096];
        if (hayTok.type == TOKEN_STRING) {
            int len = hayTok.length > 4095 ? 4094 : hayTok.length - 2;
            if (len > 0) {
                strncpy(hay, hayTok.start + 1, len);
                hay[len] = '\0';
            } else {
                hay[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(hayTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(hay, val, sizeof(hay) - 1);
            hay[sizeof(hay) - 1] = '\0';
        }
        
        if (needleTok.type == TOKEN_STRING) {
            int len = needleTok.length > 4095 ? 4094 : needleTok.length - 2;
            if (len > 0) {
                strncpy(needle, needleTok.start + 1, len);
                needle[len] = '\0';
            } else {
                needle[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(needleTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(needle, val, sizeof(needle) - 1);
            needle[sizeof(needle) - 1] = '\0';
        }
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("KittyCheckIfStringContains expects ')'");
            return 0;
        }
        
        int result = str_contains(hay, needle);
        setVar("__result", result ? 1.0 : 0.0);
        return result ? 1.0 : 0.0;
    }
    
    // ─── KittySplitString() ─────────────────────────────────────
    if (t.type == TOKEN_STRING_SPLIT) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("KittySplitString expects '('");
            return 0;
        }
        
        Token strTok = scanToken();
        Token comma = scanToken();
        Token delimTok = scanToken();
        
        if (strTok.type != TOKEN_STRING && strTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittySplitString expects string or variable");
            return 0;
        }
        if (delimTok.type != TOKEN_STRING && delimTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittySplitString expects string or variable");
            return 0;
        }
        
        char str[4096], delim[256];
        if (strTok.type == TOKEN_STRING) {
            int len = strTok.length > 4095 ? 4094 : strTok.length - 2;
            if (len > 0) {
                strncpy(str, strTok.start + 1, len);
                str[len] = '\0';
            } else {
                str[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(strTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(str, val, sizeof(str) - 1);
            str[sizeof(str) - 1] = '\0';
        }
        
        if (delimTok.type == TOKEN_STRING) {
            int len = delimTok.length > 255 ? 254 : delimTok.length - 2;
            if (len > 0) {
                strncpy(delim, delimTok.start + 1, len);
                delim[len] = '\0';
            } else {
                delim[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(delimTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(delim, val, sizeof(delim) - 1);
            delim[sizeof(delim) - 1] = '\0';
        }
        
        int count = 0;
        char** parts = split_string(str, delim, &count);
        
        for (int i = 0; i < count; i++) {
            if (parts[i]) {
                char varName[64];
                snprintf(varName, sizeof(varName), "__split_%d", i);
                setVarString(varName, parts[i]);
                free(parts[i]);
            }
        }
        if (parts) free(parts);
        
        setVar("__split_count", (double)count);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("KittySplitString expects ')'");
            return 0;
        }
        
        return (double)count;
    }
    
    // ─── KittyReplaceString() ───────────────────────────────────
    if (t.type == TOKEN_STRING_REPLACE) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("KittyReplaceString expects '('");
            return 0;
        }
        
        Token srcTok = scanToken();
        Token comma1 = scanToken();
        Token oldTok = scanToken();
        Token comma2 = scanToken();
        Token newTok = scanToken();
        
        if (srcTok.type != TOKEN_STRING && srcTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return 0;
        }
        if (oldTok.type != TOKEN_STRING && oldTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return 0;
        }
        if (newTok.type != TOKEN_STRING && newTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return 0;
        }
        
        char src[4096], old[4096], new[4096];
        
        if (srcTok.type == TOKEN_STRING) {
            int len = srcTok.length > 4095 ? 4094 : srcTok.length - 2;
            if (len > 0) {
                strncpy(src, srcTok.start + 1, len);
                src[len] = '\0';
            } else {
                src[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(srcTok, name, sizeof(name));
            strncpy(src, getVarString(name), sizeof(src) - 1);
            src[sizeof(src) - 1] = '\0';
        }
        
        if (oldTok.type == TOKEN_STRING) {
            int len = oldTok.length > 4095 ? 4094 : oldTok.length - 2;
            if (len > 0) {
                strncpy(old, oldTok.start + 1, len);
                old[len] = '\0';
            } else {
                old[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(oldTok, name, sizeof(name));
            strncpy(old, getVarString(name), sizeof(old) - 1);
            old[sizeof(old) - 1] = '\0';
        }
        
        if (newTok.type == TOKEN_STRING) {
            int len = newTok.length > 4095 ? 4094 : newTok.length - 2;
            if (len > 0) {
                strncpy(new, newTok.start + 1, len);
                new[len] = '\0';
            } else {
                new[0] = '\0';
            }
        } else {
            char name[64];
            safe_token_to_string(newTok, name, sizeof(name));
            strncpy(new, getVarString(name), sizeof(new) - 1);
            new[sizeof(new) - 1] = '\0';
        }
        
        char* result = str_replace(src, old, new);
        setVarString("__result", result);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("KittyReplaceString expects ')'");
            return 0;
        }
        
        return 1.0;
    }
    
    // ─── TRIM ──────────────────────────────────────────────────
    if (t.type == TOKEN_TRIM) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("Trim expects '('");
            return 0;
        }
        
        Token arg = scanToken();
        char* trimmed = NULL;
        
        if (arg.type == TOKEN_STRING) {
            char str[4096];
            int len = arg.length > 4095 ? 4094 : arg.length - 2;
            if (len > 0) {
                strncpy(str, arg.start + 1, len);
                str[len] = '\0';
            } else {
                str[0] = '\0';
            }
            trimmed = str_trim_copy(str);
        } else if (arg.type == TOKEN_IDENTIFIER) {
            char name[64];
            safe_token_to_string(arg, name, sizeof(name));
            trimmed = str_trim_copy(getVarString(name));
        } else {
            setErrorF("Trim expects string or variable");
            return 0;
        }
        
        setVarString("__result", trimmed);
        free(trimmed);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("Trim expects ')'");
            return 0;
        }
        
        return 1.0;
    }

    if (t.type == TOKEN_IDENTIFIER) {
        char varName[64];
        safe_token_to_string(t, varName, sizeof(varName));
        return getVar(varName);
    }

    if (t.type == TOKEN_STRING) {
        char str[4096];
        int len = t.length > 4095 ? 4094 : t.length - 2;
        if (len > 0) {
            strncpy(str, t.start + 1, len);
            str[len] = '\0';
        } else {
            str[0] = '\0';
        }
        setVarString("__result", str);
        return 0;
    }

    if (t.type == TOKEN_LPAREN) {
        double result = parse_expression();
        if (lynx_error) return 0;
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("Expected ')' after expression");
            return 0;
        }
        return result;
    }

    if (t.type == TOKEN_NOT) {
        double val = parse_primary();
        if (lynx_error) return 0;
        return val == 0 ? 1.0 : 0.0;
    }

    if (t.type == TOKEN_MINUS) {
        double val = parse_primary();
        if (lynx_error) return 0;
        return -val;
    }

    setErrorF("Unexpected token in expression");
    return 0;
}

double parse_multiplication() {
    double result = parse_primary();
    if (lynx_error) return 0;
    
    while (peekToken().type == TOKEN_STAR || peekToken().type == TOKEN_SLASH || peekToken().type == TOKEN_MODULO) {
        Token op = scanToken();
        double right = parse_primary();
        if (lynx_error) return 0;
        
        if (op.type == TOKEN_STAR) result *= right;
        else if (op.type == TOKEN_SLASH) {
            if (right == 0) {
                setErrorF("Division by zero");
                return 0;
            }
            result /= right;
        }
        else if (op.type == TOKEN_MODULO) result = (int)result % (int)right;
    }
    return result;
}

double parse_addition() {
    double result = parse_multiplication();
    if (lynx_error) return 0;
    
    while (peekToken().type == TOKEN_PLUS || peekToken().type == TOKEN_MINUS) {
        Token op = scanToken();
        double right = parse_multiplication();
        if (lynx_error) return 0;
        
        if (op.type == TOKEN_PLUS) result += right;
        else result -= right;
    }
    return result;
}

double parse_expression() {
    return parse_addition();
}

int check_condition() {
    double result = parse_expression();
    if (lynx_error) return 0;
    return result != 0;
}

int parse_logic_expression() {
    Token t = peekToken();
    
    if (t.type == TOKEN_NOT) {
        scanToken();
        int result = parse_logic_expression();
        return !result;
    }

    double left = parse_expression();
    if (lynx_error) return 0;

    Token op = peekToken();
    if (op.type == TOKEN_EQ || op.type == TOKEN_NE || 
        op.type == TOKEN_GT || op.type == TOKEN_LT || 
        op.type == TOKEN_GE || op.type == TOKEN_LE) {
        scanToken();
        double right = parse_expression();
        if (lynx_error) return 0;

        switch (op.type) {
            case TOKEN_EQ: return left == right;
            case TOKEN_NE: return left != right;
            case TOKEN_GT: return left > right;
            case TOKEN_LT: return left < right;
            case TOKEN_GE: return left >= right;
            case TOKEN_LE: return left <= right;
            default: return 0;
        }
    }

    if (peekToken().type == TOKEN_AND) {
        scanToken();
        int right = parse_logic_expression();
        return (left != 0) && right;
    }

    if (peekToken().type == TOKEN_OR) {
        scanToken();
        int right = parse_logic_expression();
        return (left != 0) || right;
    }

    return left != 0;
}

void parse_block() {
    Token lbrace = peekToken();
    if (lbrace.type == TOKEN_LBRACE) {
        scanToken();
        while (peekToken().type != TOKEN_RBRACE && peekToken().type != TOKEN_EOF) {
            parse_statement();
            if (lynx_error) break;
        }
        if (peekToken().type == TOKEN_RBRACE) scanToken();
    } else {
        parse_statement();
    }
}

void parse_for_loop() {
    Token nameToken = scanToken();
    if (nameToken.type != TOKEN_IDENTIFIER) {
        char* text = getTokenText(nameToken);
        setErrorF("For loop expects variable name, got '%s'", text);
        return;
    }
    char varName[64];
    safe_token_to_string(nameToken, varName, sizeof(varName));

    Token eq = scanToken();
    if (eq.type != TOKEN_EQUAL) {
        char* text = getTokenText(eq);
        setErrorF("For loop expects '=', got '%s'", text);
        return;
    }

    double start = parse_expression();
    if (lynx_error) return;

    Token to = scanToken();
    if (to.type != TOKEN_IDENTIFIER || strcmp(getTokenText(to), "To") != 0) {
        char* text = getTokenText(to);
        setErrorF("For loop expects 'To', got '%s'", text);
        return;
    }

    double end = parse_expression();
    if (lynx_error) return;

    Token lbrace = scanToken();
    if (lbrace.type != TOKEN_LBRACE) {
        char* text = getTokenText(lbrace);
        setErrorF("For loop expects '{', got '%s'", text);
        return;
    }

    Scanner bodyStart = scanner;
    int braceCount = 1;
    while (braceCount > 0 && peekToken().type != TOKEN_EOF) {
        Token t = scanToken();
        if (t.type == TOKEN_LBRACE) braceCount++;
        if (t.type == TOKEN_RBRACE) braceCount--;
    }
    Scanner bodyEnd = scanner;

    int bodyLen = (int)(bodyEnd.current - bodyStart.start) - 1;
    if (bodyLen < 0) bodyLen = 0;
    char* body = malloc(bodyLen + 1);
    if (!body) {
        setErrorF("Out of memory for loop body");
        return;
    }
    strncpy(body, bodyStart.start, bodyLen);
    body[bodyLen] = '\0';

    for (double i = start; i <= end && !lynx_error; i++) {
        setVar(varName, i);
        initScanner(body);
        while (peekToken().type != TOKEN_EOF) {
            parse_statement();
            if (lynx_error) break;
        }
    }
    free(body);
}

void parse_while_loop() {
    Scanner condStart = scanner;
    int condition = parse_logic_expression();
    if (lynx_error) return;
    
    Token lbrace = scanToken();
    if (lbrace.type != TOKEN_LBRACE) {
        char* text = getTokenText(lbrace);
        setErrorF("While loop expects '{' after condition, got '%s'", text);
        return;
    }
    
    Scanner bodyStart = scanner;
    int braceDepth = 1;
    while (braceDepth > 0 && peekToken().type != TOKEN_EOF) {
        Token t = scanToken();
        if (t.type == TOKEN_LBRACE) braceDepth++;
        if (t.type == TOKEN_RBRACE) braceDepth--;
    }
    Scanner bodyEnd = scanner;
    
    int bodyLen = (int)(bodyEnd.current - bodyStart.start) - 1;
    if (bodyLen < 0) bodyLen = 0;
    char* body = malloc(bodyLen + 1);
    if (!body) {
        setErrorF("Out of memory for loop body");
        return;
    }
    strncpy(body, bodyStart.start, bodyLen);
    body[bodyLen] = '\0';
    
    while (condition && !lynx_error) {
        initScanner(body);
        while (peekToken().type != TOKEN_EOF) {
            parse_statement();
            if (lynx_error) break;
        }
        if (lynx_error) break;
        initScanner(condStart.start);
        condition = parse_logic_expression();
    }
    free(body);
}

void parse_function_def() {
    Token nameToken = scanToken();
    if (nameToken.type != TOKEN_IDENTIFIER) {
        char* text = getTokenText(nameToken);
        setErrorF("Function definition expects function name, got '%s'", text);
        return;
    }
    char funcName[64];
    safe_token_to_string(nameToken, funcName, sizeof(funcName));
    
    Token lparen = scanToken();
    if (lparen.type != TOKEN_LPAREN) {
        char* text = getTokenText(lparen);
        setErrorF("Function definition expects '(' after function name, got '%s'", text);
        return;
    }
    
    char params[10][64];
    int paramCount = 0;
    while (peekToken().type != TOKEN_RPAREN && peekToken().type != TOKEN_EOF) {
        Token param = scanToken();
        if (param.type == TOKEN_IDENTIFIER) {
            if (paramCount < 10) {
                safe_token_to_string(param, params[paramCount], sizeof(params[paramCount]));
                paramCount++;
            }
        } else {
            char* text = getTokenText(param);
            setErrorF("Function parameter must be identifier, got '%s'", text);
            return;
        }
        if (peekToken().type == TOKEN_COMMA) scanToken();
    }
    if (peekToken().type == TOKEN_RPAREN) {
        scanToken();
    } else {
        setErrorF("Function definition expects ')' after parameters");
        return;
    }
    
    Scanner bodyStart = scanner;
    Token brace = scanToken();
    if (brace.type != TOKEN_LBRACE) {
        char* text = getTokenText(brace);
        setErrorF("Function definition expects '{' after parameters, got '%s'", text);
        return;
    }
    
    int braceCount = 1;
    while (braceCount > 0 && peekToken().type != TOKEN_EOF) {
        Token t = scanToken();
        if (t.type == TOKEN_LBRACE) braceCount++;
        if (t.type == TOKEN_RBRACE) braceCount--;
    }
    int bodyLen = (int)(scanner.current - bodyStart.current) - 1;
    if (bodyLen < 0) bodyLen = 0;
    char* body = malloc(bodyLen + 1);
    if (!body) {
        setErrorF("Out of memory for function body");
        return;
    }
    strncpy(body, bodyStart.current, bodyLen);
    body[bodyLen] = '\0';
    defineFunction(funcName, (const char**)params, paramCount, body);
}

// ─── FORMAT / CHECK ──────────────────────────────────────────────
void format_file(const char* path) {
    if (!path || strlen(path) >= LYNX_MAX_PATH) {
        setErrorF("Invalid file path");
        printf("%s\n", lynx_error);
        clearError();
        return;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        setErrorF("Cannot open file '%s' for formatting", path);
        printf("%s\n", lynx_error);
        clearError();
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    
    if (size < 0 || size > 1000000) {
        fprintf(stderr, "🐾 ERROR: File size invalid (must be 0-1MB)\n");
        fclose(f);
        return;
    }
    
    char* src = malloc(size + 1);
    if (!src) {
        fprintf(stderr, "🐾 ERROR: Out of memory reading file\n");
        fclose(f);
        return;
    }
    
    size_t bytesRead = fread(src, 1, size, f);
    fclose(f);
    
    if (bytesRead != (size_t)size) {
        fprintf(stderr, "🐾 ERROR: Could not read entire file\n");
        free(src);
        return;
    }
    
    src[size] = '\0';

    char* result = malloc(size * 2 + 1);
    if (!result) {
        fprintf(stderr, "🐾 ERROR: Out of memory for formatting\n");
        free(src);
        return;
    }
    
    result[0] = '\0';
    int indent = 0;
    int line_start = 1;
    size_t resultLen = 0;
    size_t maxLen = size * 2;

    for (int i = 0; src[i] && resultLen < maxLen; i++) {
        char c = src[i];
        if (c == '\n') {
            if (resultLen + 1 < maxLen) {
                result[resultLen++] = '\n';
            }
            line_start = 1;
        } else if (line_start) {
            for (int j = 0; j < indent * 2 && resultLen < maxLen; j++) {
                result[resultLen++] = ' ';
            }
            line_start = 0;
            for (int j = i; src[j] && src[j] != '\n'; j++) {
                if (src[j] == '{') indent++;
                else if (src[j] == '}') indent--;
            }
            while (src[i] && src[i] != '\n' && resultLen < maxLen) {
                result[resultLen++] = src[i++];
            }
            if (src[i]) i--;
        }
    }
    result[resultLen] = '\0';
    
    if (resultLen > 0 && result[resultLen-1] != '\n' && resultLen + 1 < maxLen) {
        result[resultLen++] = '\n';
        result[resultLen] = '\0';
    }

    f = fopen(path, "w");
    if (f) {
        fwrite(result, 1, resultLen, f);
        fclose(f);
        printf("🐾 Formatted %s\n", path);
    } else {
        setErrorF("Cannot write to file '%s'", path);
        printf("%s\n", lynx_error);
        clearError();
    }

    free(src);
    free(result);
}

void check_file(const char* path) {
    if (!path || strlen(path) >= LYNX_MAX_PATH) {
        setErrorF("Invalid file path");
        printf("%s\n", lynx_error);
        clearError();
        return;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        setErrorF("Cannot open file '%s' for checking", path);
        printf("%s\n", lynx_error);
        clearError();
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    
    if (size < 0 || size > 1000000) {
        fprintf(stderr, "🐾 ERROR: File size invalid (must be 0-1MB)\n");
        fclose(f);
        return;
    }
    
    char* src = malloc(size + 1);
    if (!src) {
        fprintf(stderr, "🐾 ERROR: Out of memory reading file\n");
        fclose(f);
        return;
    }
    
    size_t bytesRead = fread(src, 1, size, f);
    fclose(f);
    
    if (bytesRead != (size_t)size) {
        fprintf(stderr, "🐾 ERROR: Could not read entire file\n");
        free(src);
        return;
    }
    
    src[size] = '\0';

    char* old_error = lynx_error ? strdup(lynx_error) : NULL;
    clearError();

    initScanner(src);
    while (peekToken().type != TOKEN_EOF) {
        parse_statement();
        if (lynx_error) break;
    }

    if (lynx_error) {
        printf("🐾 Error: %s\n", lynx_error);
        clearError();
    } else {
        printf("✅ No errors found in %s\n", path);
    }

    if (old_error) {
        lynx_error = old_error;
    }

    free(src);
}

// ─── PARSE STATEMENT ──────────────────────────────────────────
void parse_statement() {
    Token t = scanToken();

    if (t.type == TOKEN_IF) {
        int cond = parse_logic_expression();
        if (lynx_error) return;
        
        if (cond) {
            parse_block();
        } else {
            Scanner save = scanner;
            parse_block();
            scanner = save;
            if (peekToken().type == TOKEN_ELSE) {
                scanToken();
                if (peekToken().type == TOKEN_IF) {
                    parse_statement();
                } else {
                    parse_block();
                }
            }
        }
        return;
    }

    if (t.type == TOKEN_FOR) { parse_for_loop(); return; }
    if (t.type == TOKEN_WHILE) { parse_while_loop(); return; }
    if (t.type == TOKEN_FUNC) { parse_function_def(); return; }
    
    if (t.type == TOKEN_TRY) {
        extern void parse_try_catch(void);
        parse_try_catch();
        return;
    }

    extern int pawcom_parse_statement(Token t);
    if (pawcom_parse_statement(t)) {
        return;
    }

    if (t.type == TOKEN_HELP || t.type == TOKEN_EOF) return;
    
    char* text = getTokenText(t);
    const char* typeName = tokenTypeToString(t.type);
    setErrorF("Unexpected '%s' (type: %s). Expected a command.",
              text ? text : "unknown", typeName ? typeName : "unknown");
    printf("%s\n", lynx_error);
    clearError();
}
