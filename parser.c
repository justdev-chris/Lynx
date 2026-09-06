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

// Unescape a string token (handles \", \\, \n, \t, \r)
static void unescape_string_token(Token t, char* out, size_t outlen) {
    if (!out || outlen < 1) return;
    out[0] = '\0';
    if (t.type != TOKEN_STRING || !t.start || t.length < 2) return;

    const char* src = t.start + 1;          // skip opening "
    int srcLen = t.length - 2;              // exclude both quotes
    size_t j = 0;

    for (int i = 0; i < srcLen && j < outlen - 1; i++) {
        if (src[i] == '\\' && i + 1 < srcLen) {
            char esc = src[i + 1];
            switch (esc) {
                case 'n':  out[j++] = '\n'; break;
                case 't':  out[j++] = '\t'; break;
                case 'r':  out[j++] = '\r'; break;
                case '\\': out[j++] = '\\'; break;
                case '"':  out[j++] = '"';  break;
                default:   out[j++] = esc;  break;
            }
            i++; // skip the escaped character
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
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

// ─── EXPRESSION PARSING (returns Value) ────────────────────────

static Value parse_primary() {
    Value result = {0};
    result.type = VAR_NUMBER;
    result.value.numValue = 0;
    
    Token t = scanToken();
    
    if (t.type == TOKEN_NUMBER) {
        result.type = VAR_NUMBER;
        result.value.numValue = atof(t.start);
        return result;
    }
    
    if (t.type == TOKEN_STRING) {
        char str[4096];
        unescape_string_token(t, str, sizeof(str));
        result.type = VAR_STRING;
        result.value.strValue = malloc(strlen(str) + 1);
        if (result.value.strValue) {
            strcpy(result.value.strValue, str);
        } else {
            result.type = VAR_NUMBER;
            result.value.numValue = 0;
        }
        return result;
    }
    
    if (t.type == TOKEN_IDENTIFIER) {
        char varName[64];
        safe_token_to_string(t, varName, sizeof(varName));
        Variable* v = findVar(varName);
        if (v) {
            if (v->type == VAR_NUMBER) {
                result.type = VAR_NUMBER;
                result.value.numValue = v->value.numValue;
            } else if (v->type == VAR_STRING) {
                result.type = VAR_STRING;
                result.value.strValue = malloc(strlen(v->value.strValue) + 1);
                if (result.value.strValue) {
                    strcpy(result.value.strValue, v->value.strValue);
                } else {
                    result.type = VAR_NUMBER;
                    result.value.numValue = 0;
                }
            }
        }
        return result;
    }
    
    // ─── LEN() ──────────────────────────────────────────────────
    if (t.type == TOKEN_LEN) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("Len expects '('");
            return result;
        }
        
        Token arg = scanToken();
        char str[4096] = {0};
        if (arg.type == TOKEN_IDENTIFIER) {
            char name[64];
            safe_token_to_string(arg, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(str, val, sizeof(str) - 1);
        } else if (arg.type == TOKEN_STRING) {
            unescape_string_token(arg, str, sizeof(str));
        } else {
            setErrorF("Len expects a string or variable name");
        }
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("Len expects ')'");
        }
        
        double resultVal = (double)strlen(str);
        setVar("__result", resultVal);
        result.type = VAR_NUMBER;
        result.value.numValue = resultVal;
        return result;
    }
    
    // ─── getenv() ──────────────────────────────────────────────
    if (t.type == TOKEN_GETENV) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("getenv expects '('");
            return result;
        }
        
        Token arg = scanToken();
        if (arg.type != TOKEN_STRING) {
            setErrorF("getenv expects a string");
            return result;
        }
        
        char name[256];
        unescape_string_token(arg, name, sizeof(name));
        const char* value = getenv(name);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("getenv expects ')'");
            return result;
        }
        
        if (value) {
            setVarString("__result", value);
            result.type = VAR_STRING;
            result.value.strValue = malloc(strlen(value) + 1);
            if (result.value.strValue) {
                strcpy(result.value.strValue, value);
            }
        } else {
            setVarString("__result", "");
            result.type = VAR_STRING;
            result.value.strValue = malloc(1);
            if (result.value.strValue) {
                result.value.strValue[0] = '\0';
            }
        }
        return result;
    }
    
    // ─── KittyCheckIfStringContains() ──────────────────────────
    if (t.type == TOKEN_STRING_CONTAINS) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("KittyCheckIfStringContains expects '('");
            return result;
        }
        
        Token hayTok = scanToken();
        scanToken(); // comma
        Token needleTok = scanToken();
        
        if (hayTok.type != TOKEN_STRING && hayTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyCheckIfStringContains expects string or variable");
            return result;
        }
        if (needleTok.type != TOKEN_STRING && needleTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyCheckIfStringContains expects string or variable");
            return result;
        }
        
        char hay[4096] = {0}, needle[4096] = {0};
        if (hayTok.type == TOKEN_STRING) {
            unescape_string_token(hayTok, hay, sizeof(hay));
        } else {
            char name[64];
            safe_token_to_string(hayTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(hay, val, sizeof(hay) - 1);
        }
        
        if (needleTok.type == TOKEN_STRING) {
            unescape_string_token(needleTok, needle, sizeof(needle));
        } else {
            char name[64];
            safe_token_to_string(needleTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(needle, val, sizeof(needle) - 1);
        }
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("KittyCheckIfStringContains expects ')'");
            return result;
        }
        
        int contains = str_contains(hay, needle);
        setVar("__result", contains ? 1.0 : 0.0);
        result.type = VAR_NUMBER;
        result.value.numValue = contains ? 1.0 : 0.0;
        return result;
    }
    
    // ─── KittySplitString() ─────────────────────────────────────
    if (t.type == TOKEN_STRING_SPLIT) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("KittySplitString expects '('");
            return result;
        }
        
        Token strTok = scanToken();
        scanToken(); // comma
        Token delimTok = scanToken();
        
        if (strTok.type != TOKEN_STRING && strTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittySplitString expects string or variable");
            return result;
        }
        if (delimTok.type != TOKEN_STRING && delimTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittySplitString expects string or variable");
            return result;
        }
        
        char str[4096] = {0}, delim[256] = {0};
        if (strTok.type == TOKEN_STRING) {
            unescape_string_token(strTok, str, sizeof(str));
        } else {
            char name[64];
            safe_token_to_string(strTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(str, val, sizeof(str) - 1);
        }
        
        if (delimTok.type == TOKEN_STRING) {
            unescape_string_token(delimTok, delim, sizeof(delim));
        } else {
            char name[64];
            safe_token_to_string(delimTok, name, sizeof(name));
            char* val = getVarString(name);
            strncpy(delim, val, sizeof(delim) - 1);
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
            return result;
        }
        
        result.type = VAR_NUMBER;
        result.value.numValue = (double)count;
        return result;
    }
    
    // ─── KittyReplaceString() ───────────────────────────────────
    if (t.type == TOKEN_STRING_REPLACE) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("KittyReplaceString expects '('");
            return result;
        }
        
        Token srcTok = scanToken();
        scanToken(); // comma1
        Token oldTok = scanToken();
        scanToken(); // comma2
        Token newTok = scanToken();
        
        if (srcTok.type != TOKEN_STRING && srcTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return result;
        }
        if (oldTok.type != TOKEN_STRING && oldTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return result;
        }
        if (newTok.type != TOKEN_STRING && newTok.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return result;
        }
        
        char src[4096] = {0}, old[4096] = {0}, new[4096] = {0};
        
        if (srcTok.type == TOKEN_STRING) {
            unescape_string_token(srcTok, src, sizeof(src));
        } else {
            char name[64];
            safe_token_to_string(srcTok, name, sizeof(name));
            strncpy(src, getVarString(name), sizeof(src) - 1);
        }
        
        if (oldTok.type == TOKEN_STRING) {
            unescape_string_token(oldTok, old, sizeof(old));
        } else {
            char name[64];
            safe_token_to_string(oldTok, name, sizeof(name));
            strncpy(old, getVarString(name), sizeof(old) - 1);
        }
        
        if (newTok.type == TOKEN_STRING) {
            unescape_string_token(newTok, new, sizeof(new));
        } else {
            char name[64];
            safe_token_to_string(newTok, name, sizeof(name));
            strncpy(new, getVarString(name), sizeof(new) - 1);
        }
        
        char* replaced = str_replace(src, old, new);
        setVarString("__result", replaced);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("KittyReplaceString expects ')'");
            return result;
        }
        
        result.type = VAR_STRING;
        result.value.strValue = malloc(strlen(replaced) + 1);
        if (result.value.strValue) {
            strcpy(result.value.strValue, replaced);
        }
        return result;
    }
    
    // ─── TRIM ──────────────────────────────────────────────────
    if (t.type == TOKEN_TRIM) {
        Token lparen = scanToken();
        if (lparen.type != TOKEN_LPAREN) {
            setErrorF("Trim expects '('");
            return result;
        }
        
        Token arg = scanToken();
        char* trimmed = NULL;
        
        if (arg.type == TOKEN_STRING) {
            char str[4096];
            unescape_string_token(arg, str, sizeof(str));
            trimmed = str_trim_copy(str);
        } else if (arg.type == TOKEN_IDENTIFIER) {
            char name[64];
            safe_token_to_string(arg, name, sizeof(name));
            trimmed = str_trim_copy(getVarString(name));
        } else {
            setErrorF("Trim expects string or variable");
            return result;
        }
        
        setVarString("__result", trimmed);
        
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("Trim expects ')'");
            free(trimmed);
            return result;
        }
        
        result.type = VAR_STRING;
        result.value.strValue = trimmed; // str_trim_copy already malloc'd
        return result;
    }

    if (t.type == TOKEN_LPAREN) {
        Value val = parse_expression();
        if (lynx_error) return result;
        Token rparen = scanToken();
        if (rparen.type != TOKEN_RPAREN) {
            setErrorF("Expected ')' after expression");
        }
        return val;
    }

    if (t.type == TOKEN_NOT) {
        Value val = parse_primary();
        if (val.type == VAR_NUMBER) {
            val.value.numValue = (val.value.numValue == 0) ? 1.0 : 0.0;
        } else {
            setErrorF("Cannot use Not on a string");
        }
        return val;
    }

    if (t.type == TOKEN_MINUS) {
        Value val = parse_primary();
        if (val.type == VAR_NUMBER) {
            val.value.numValue = -val.value.numValue;
        } else {
            setErrorF("Cannot negate a string");
        }
        return val;
    }

    setErrorF("Unexpected token in expression");
    return result;
}

static Value parse_multiplication() {
    Value result = parse_primary();
    if (lynx_error) return result;
    
    while (peekToken().type == TOKEN_STAR || peekToken().type == TOKEN_SLASH || peekToken().type == TOKEN_MODULO) {
        Token op = scanToken();
        Value right = parse_primary();
        if (lynx_error) return result;
        
        if (result.type != VAR_NUMBER || right.type != VAR_NUMBER) {
            setErrorF("Cannot use * / %% on strings");
            return result;
        }
        
        if (op.type == TOKEN_STAR) {
            result.value.numValue *= right.value.numValue;
        } else if (op.type == TOKEN_SLASH) {
            if (right.value.numValue == 0) {
                setErrorF("Division by zero");
                return result;
            }
            result.value.numValue /= right.value.numValue;
        } else if (op.type == TOKEN_MODULO) {
            result.value.numValue = (int)result.value.numValue % (int)right.value.numValue;
        }
    }
    return result;
}

static Value parse_addition() {
    Value result = parse_multiplication();
    if (lynx_error) return result;
    
    while (peekToken().type == TOKEN_PLUS || peekToken().type == TOKEN_MINUS) {
        Token op = scanToken();
        Value right = parse_multiplication();
        if (lynx_error) return result;
        
        if (op.type == TOKEN_PLUS) {
            // String concatenation
            if (result.type == VAR_STRING || right.type == VAR_STRING) {
                // Convert both to strings
                char leftStr[4096] = {0};
                char rightStr[4096] = {0};
                
                if (result.type == VAR_STRING && result.value.strValue) {
                    strncpy(leftStr, result.value.strValue, sizeof(leftStr) - 1);
                } else if (result.type == VAR_NUMBER) {
                    snprintf(leftStr, sizeof(leftStr), "%.5f", result.value.numValue);
                }
                
                if (right.type == VAR_STRING && right.value.strValue) {
                    strncpy(rightStr, right.value.strValue, sizeof(rightStr) - 1);
                } else if (right.type == VAR_NUMBER) {
                    snprintf(rightStr, sizeof(rightStr), "%.5f", right.value.numValue);
                }
                
                // Free old string if any
                if (result.type == VAR_STRING && result.value.strValue) {
                    free(result.value.strValue);
                    result.value.strValue = NULL;
                }
                
                result.type = VAR_STRING;
                result.value.strValue = malloc(strlen(leftStr) + strlen(rightStr) + 1);
                if (result.value.strValue) {
                    strcpy(result.value.strValue, leftStr);
                    strcat(result.value.strValue, rightStr);
                } else {
                    result.type = VAR_NUMBER;
                    result.value.numValue = 0;
                }
            } else {
                // Both are numbers
                result.value.numValue += right.value.numValue;
            }
        } else { // MINUS
            if (result.type != VAR_NUMBER || right.type != VAR_NUMBER) {
                setErrorF("Cannot subtract strings");
                return result;
            }
            result.value.numValue -= right.value.numValue;
        }
    }
    return result;
}

Value parse_expression() {
    return parse_addition();
}

int check_condition() {
    Value result = parse_expression();
    if (lynx_error) return 0;
    if (result.type == VAR_STRING) {
        return result.value.strValue != NULL && strlen(result.value.strValue) > 0;
    }
    return result.value.numValue != 0;
}

// ─── LOGIC EXPRESSION ───────────────────────────────────────────

int parse_logic_expression() {
    Token t = peekToken();
    
    if (t.type == TOKEN_NOT) {
        scanToken();
        int result = parse_logic_expression();
        return !result;
    }

    // Parse left side as a Value
    Value left = parse_expression();
    if (lynx_error) return 0;

    // Check for comparison operators
    Token op = peekToken();
    if (op.type == TOKEN_EQ || op.type == TOKEN_NE || 
        op.type == TOKEN_GT || op.type == TOKEN_LT || 
        op.type == TOKEN_GE || op.type == TOKEN_LE) {
        scanToken();
        Value right = parse_expression();
        if (lynx_error) return 0;

        // Handle string comparisons
        if (left.type == VAR_STRING || right.type == VAR_STRING) {
            char leftStr[4096] = {0};
            char rightStr[4096] = {0};
            
            if (left.type == VAR_STRING && left.value.strValue) {
                strncpy(leftStr, left.value.strValue, sizeof(leftStr) - 1);
            } else if (left.type == VAR_NUMBER) {
                snprintf(leftStr, sizeof(leftStr), "%.5f", left.value.numValue);
            }
            
            if (right.type == VAR_STRING && right.value.strValue) {
                strncpy(rightStr, right.value.strValue, sizeof(rightStr) - 1);
            } else if (right.type == VAR_NUMBER) {
                snprintf(rightStr, sizeof(rightStr), "%.5f", right.value.numValue);
            }
            
            int cmp = strcmp(leftStr, rightStr);
            switch (op.type) {
                case TOKEN_EQ: return cmp == 0;
                case TOKEN_NE: return cmp != 0;
                case TOKEN_GT: return cmp > 0;
                case TOKEN_LT: return cmp < 0;
                case TOKEN_GE: return cmp >= 0;
                case TOKEN_LE: return cmp <= 0;
                default: return 0;
            }
        }

        // Numeric comparisons
        double l = left.type == VAR_NUMBER ? left.value.numValue : 0;
        double r = right.type == VAR_NUMBER ? right.value.numValue : 0;
        
        switch (op.type) {
            case TOKEN_EQ: return l == r;
            case TOKEN_NE: return l != r;
            case TOKEN_GT: return l > r;
            case TOKEN_LT: return l < r;
            case TOKEN_GE: return l >= r;
            case TOKEN_LE: return l <= r;
            default: return 0;
        }
    }

    // AND / OR
    if (peekToken().type == TOKEN_AND) {
        scanToken();
        int right = parse_logic_expression();
        return (left.type == VAR_NUMBER && left.value.numValue != 0) && right;
    }

    if (peekToken().type == TOKEN_OR) {
        scanToken();
        int right = parse_logic_expression();
        return (left.type == VAR_NUMBER && left.value.numValue != 0) || right;
    }

    // For non-numeric values, treat as true if they exist
    if (left.type == VAR_STRING && left.value.strValue) {
        return 1; // Non-empty string is true
    }
    
    return (left.type == VAR_NUMBER && left.value.numValue != 0);
}

// ─── BLOCK PARSING ──────────────────────────────────────────────

static void parse_block_ex(int execute) {
    Token lbrace = peekToken();
    if (lbrace.type == TOKEN_LBRACE) {
        scanToken();
        if (execute) {
            while (peekToken().type != TOKEN_RBRACE && peekToken().type != TOKEN_EOF) {
                parse_statement();
                if (lynx_error) break;
                if (lynx_return_flag) return;
            }
        } else {
            int depth = 1;
            while (depth > 0 && peekToken().type != TOKEN_EOF) {
                Token t = scanToken();
                if (t.type == TOKEN_LBRACE) depth++;
                if (t.type == TOKEN_RBRACE) depth--;
            }
            return;
        }
        if (peekToken().type == TOKEN_RBRACE) scanToken();
    } else {
        if (execute) {
            parse_statement();
        } else {
            scanToken();
        }
    }
}

void parse_block() {
    parse_block_ex(1);
}

// ─── LOOPS ──────────────────────────────────────────────────────

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

    Value startVal = parse_expression();
    if (lynx_error) return;
    double start = startVal.type == VAR_NUMBER ? startVal.value.numValue : 0;

    Token to = scanToken();
    if (to.type != TOKEN_IDENTIFIER || strcmp(getTokenText(to), "To") != 0) {
        char* text = getTokenText(to);
        setErrorF("For loop expects 'To', got '%s'", text);
        return;
    }

    Value endVal = parse_expression();
    if (lynx_error) return;
    double end = endVal.type == VAR_NUMBER ? endVal.value.numValue : 0;

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
            if (lynx_return_flag) {
                lynx_return_flag = 0;
                free(body);
                return;
            }
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
            if (lynx_return_flag) {
                lynx_return_flag = 0;
                free(body);
                return;
            }
        }
        if (lynx_error) break;
        initScanner(condStart.start);
        condition = parse_logic_expression();
    }
    free(body);
}

// ─── FUNCTIONS ──────────────────────────────────────────────────

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
    free(body);
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

    // Handle Return statement
    if (t.type == TOKEN_RETURN) {
        Token next = peekToken();
        double returnValue = 0;
        
        if (next.type != TOKEN_EOF && next.type != TOKEN_SEMICOLON) {
            Value val = parse_expression();
            if (lynx_error) return;
            returnValue = val.type == VAR_NUMBER ? val.value.numValue : 0;
        }
        
        Token semi = scanToken();
        if (semi.type != TOKEN_EOF && semi.type != TOKEN_SEMICOLON) {
            setErrorF("Return expects ';' after expression");
            return;
        }
        
        lynx_return_flag = 1;
        lynx_return_value = returnValue;
        return;
    }

    if (t.type == TOKEN_IF) {
        int cond = parse_logic_expression();
        if (lynx_error) return;
        
        if (cond) {
            parse_block_ex(1);
            
            while (peekToken().type == TOKEN_ELSE) {
                scanToken();
                
                if (peekToken().type == TOKEN_IF) {
                    scanToken();
                    parse_logic_expression();
                    if (lynx_error) clearError();
                    parse_block_ex(0);
                } else {
                    parse_block_ex(0);
                    break;
                }
            }
        } else {
            parse_block_ex(0);
            
            if (peekToken().type == TOKEN_ELSE) {
                scanToken();
                
                if (peekToken().type == TOKEN_IF) {
                    parse_statement();
                } else {
                    parse_block_ex(1);
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