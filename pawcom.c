#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include "lynx.h"
#include "platform.h"

#ifdef _WIN32
#include <direct.h>
#endif

extern Scanner scanner;
extern char* lynx_error;
extern char* loaded_packages[64];
extern int loaded_pkg_count;
extern TryState try_state;

extern double parse_expression();
extern void parse_block();
extern void setVar(const char* name, double value);
extern void setVarString(const char* name, const char* value);
extern double getVar(const char* name);
extern char* getVarString(const char* name);
extern void pounce(const char* name);
extern void hunt();
extern void load_lib(const char* lib_name);
extern void runFile(const char* path, int argc, char** argv);
extern void setError(const char* msg, int line, int col);
extern void setErrorF(const char* format, ...);
extern void clearError();
extern const char* tokenTypeToString(LynxTokenType type);
extern char* getTokenText(Token t);
extern Variable* findVar(const char* name);
extern Variable den[];
extern int varCount;

// Thread-local storage for str_replace result (caller must use immediately)
#define STR_REPLACE_BUFFER_SIZE 65536
static char str_replace_buffer[STR_REPLACE_BUFFER_SIZE];

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

static void parse_array() {
    Token nameToken = scanToken();
    if (nameToken.type != TOKEN_IDENTIFIER) {
        char* text = getTokenText(nameToken);
        setErrorF("Array assignment expects variable name, got '%s' (type: %s)", 
                  text, tokenTypeToString(nameToken.type));
        return;
    }
    char varName[64];
    snprintf(varName, sizeof(varName), "%.*s", nameToken.length, nameToken.start);

    Token op = scanToken();
    if (op.type != TOKEN_EQUAL) {
        char* text = getTokenText(op);
        setErrorF("Array assignment expects '=', got '%s' (type: %s)", 
                  text, tokenTypeToString(op.type));
        return;
    }

    Token bracket = scanToken();
    if (bracket.type != TOKEN_LBRACKET) {
        char* text = getTokenText(bracket);
        setErrorF("Array assignment expects '[', got '%s' (type: %s)", 
                  text, tokenTypeToString(bracket.type));
        return;
    }

    int count = 0;
    double values[256];
    while (peekToken().type != TOKEN_RBRACKET && peekToken().type != TOKEN_EOF) {
        double val = parse_expression();
        if (lynx_error) return;
        if (count < 256) {
            values[count++] = val;
        }
        if (peekToken().type == TOKEN_COMMA) scanToken();
    }

    if (peekToken().type == TOKEN_RBRACKET) {
        scanToken();
    } else {
        setErrorF("Array assignment expects ']' to close array");
        return;
    }

    printf("Array %s created with %d elements\n", varName, count);
}

void parse_try_catch() {
    try_state.is_trying = 1;
    try_state.caught = 0;
    try_state.error_message = NULL;
    try_state.error_line = 0;
    try_state.error_col = 0;
    
    if (setjmp(try_state.env) == 0) {
        Token brace = scanToken();
        if (brace.type != TOKEN_LBRACE) {
            char* text = getTokenText(brace);
            setErrorF("Try expects '{', got '%s' (type: %s)", 
                      text, tokenTypeToString(brace.type));
            try_state.is_trying = 0;
            return;
        }
        
        while (peekToken().type != TOKEN_RBRACE && peekToken().type != TOKEN_EOF) {
            parse_statement();
            if (lynx_error && !try_state.is_trying) break;
        }
        if (peekToken().type == TOKEN_RBRACE) scanToken();
        
        if (!lynx_error) {
            try_state.is_trying = 0;
            if (peekToken().type == TOKEN_CATCH) {
                scanToken();
                Token brace2 = scanToken();
                if (brace2.type == TOKEN_LBRACE) {
                    int depth = 1;
                    while (depth > 0 && peekToken().type != TOKEN_EOF) {
                        Token t = scanToken();
                        if (t.type == TOKEN_LBRACE) depth++;
                        if (t.type == TOKEN_RBRACE) depth--;
                    }
                }
            }
        } else {
            try_state.is_trying = 0;
        }
    } else {
        try_state.is_trying = 0;
        try_state.caught = 1;
        clearError();
        
        Token next = peekToken();
        if (next.type != TOKEN_CATCH) {
            if (try_state.error_message) {
                fprintf(stderr, "🐾 %s\n", try_state.error_message);
            }
            return;
        }
        scanToken();
        
        Token brace = scanToken();
        if (brace.type != TOKEN_LBRACE) {
            char* text = getTokenText(brace);
            setErrorF("Catch expects '{', got '%s' (type: %s)", 
                      text, tokenTypeToString(brace.type));
            return;
        }
        
        while (peekToken().type != TOKEN_RBRACE && peekToken().type != TOKEN_EOF) {
            parse_statement();
            if (lynx_error) break;
        }
        if (peekToken().type == TOKEN_RBRACE) scanToken();
        clearError();
    }
}

static void kitty_port(const char* name) {
    if (!name || strlen(name) > 240) {
        setErrorF("KittyPort: Invalid package name");
        return;
    }
    
    char lnxPath[LYNX_MAX_PATH];
    snprintf(lnxPath, sizeof(lnxPath), "libs/%s/main.lnx", name);

    int alreadyLoaded = 0;
    for (int i = 0; i < loaded_pkg_count; i++) {
        if (loaded_packages[i] && strcmp(loaded_packages[i], name) == 0) { 
            alreadyLoaded = 1; 
            break; 
        }
    }

    FILE* f = fopen(lnxPath, "r");
    if (f) {
        fclose(f);
        if (alreadyLoaded) return;

        Variable* savedDen = malloc(varCount * sizeof(Variable));
        if (!savedDen) {
            setErrorF("Out of memory in KittyPort");
            return;
        }
        
        int savedCount = varCount;
        for (int i = 0; i < varCount; i++) {
            savedDen[i] = den[i];
        }

        runFile(lnxPath, 0, NULL);

        for (int i = 0; i < varCount; i++) {
            if (den[i].value.strValue) free(den[i].value.strValue);
        }
        for (int i = 0; i < savedCount; i++) den[i] = savedDen[i];
        varCount = savedCount;
        free(savedDen);

        if (loaded_pkg_count < 64) {
            loaded_packages[loaded_pkg_count++] = strdup(name);
        }
        return;
    }

    char dllPath[LYNX_MAX_PATH];
    snprintf(dllPath, sizeof(dllPath), "lib/%s.dll", name);
    f = fopen(dllPath, "r");
    if (f) {
        fclose(f);
        load_lib(name);
        return;
    }

    setErrorF("KittyPort: Package '%s' not found in libs/ or lib/", name);
    printf("%s\n", lynx_error);
    clearError();
}

int pawcom_parse_statement(Token t) {
    if (t.type == TOKEN_HUNT) { hunt(); return 1; }

    // ─── ROAR ──────────────────────────────────────────────────
    if (t.type == TOKEN_ROAR) {
        char result[4096] = "";
        
        while (1) {
            Token val = scanToken();
            
            if (val.type == TOKEN_EOF || val.type == TOKEN_ERROR) {
                break;
            }
            
            if (val.type == TOKEN_STRING) {
                if (val.length > 1) {
                    char str[4096];
                    snprintf(str, sizeof(str), "%.*s", val.length - 2, val.start + 1);
                    strncat(result, str, sizeof(result) - strlen(result) - 1);
                }
            } else if (val.type == TOKEN_IDENTIFIER) {
                char name[64];
                snprintf(name, sizeof(name), "%.*s", val.length, val.start);
                char* s = getVarString(name);
                if (s && strlen(s) > 0) {
                    strncat(result, s, sizeof(result) - strlen(result) - 1);
                } else {
                    double num = getVar(name);
                    if (lynx_error) {
                        printf("%s\n", lynx_error);
                        clearError();
                        return 1;
                    }
                    char numStr[32];
                    snprintf(numStr, sizeof(numStr), "%.5f", num);
                    strncat(result, numStr, sizeof(result) - strlen(result) - 1);
                }
            } else if (val.type == TOKEN_NUMBER) {
                char numStr[32];
                snprintf(numStr, sizeof(numStr), "%.5f", atof(val.start));
                strncat(result, numStr, sizeof(result) - strlen(result) - 1);
            } else {
                if (val.type != TOKEN_PLUS) {
                    char* text = getTokenText(val);
                    setErrorF("Roar expects string, identifier, or number, got '%s' (type: %s)",
                              text, tokenTypeToString(val.type));
                    printf("%s\n", lynx_error);
                    clearError();
                    return 1;
                }
                Token next = peekToken();
                if (next.type == TOKEN_EOF || next.type == TOKEN_ERROR) break;
            }
        }
        printf("%s\n", result);
        return 1;
    }

    // ─── SET ──────────────────────────────────────────────────
    if (t.type == TOKEN_SET) {
        Token nameToken = scanToken();
        if (nameToken.type != TOKEN_IDENTIFIER) {
            char* text = getTokenText(nameToken);
            setErrorF("Set expects identifier, got '%s' (type: %s)", 
                      text, tokenTypeToString(nameToken.type));
            return 1;
        }
        char varName[64];
        snprintf(varName, sizeof(varName), "%.*s", nameToken.length, nameToken.start);

        Token op = scanToken();
        if (op.type != TOKEN_EQUAL) {
            char* text = getTokenText(op);
            setErrorF("Set expects '=', got '%s' (type: %s)", 
                      text, tokenTypeToString(op.type));
            return 1;
        }

        double value = parse_expression();
        if (lynx_error) return 1;

        setVar(varName, value);
        return 1;
    }

    // ─── POUNCE ────────────────────────────────────────────────
    if (t.type == TOKEN_POUNCE) {
        Token nameToken = scanToken();
        if (nameToken.type != TOKEN_IDENTIFIER) {
            char* text = getTokenText(nameToken);
            setErrorF("Pounce expects identifier, got '%s' (type: %s)", 
                      text, tokenTypeToString(nameToken.type));
            return 1;
        }
        char varName[64];
        snprintf(varName, sizeof(varName), "%.*s", nameToken.length, nameToken.start);
        pounce(varName);
        return 1;
    }

    // ─── KITTY_PORT ────────────────────────────────────────────
    if (t.type == TOKEN_KITTY_PORT) {
        Token pkgToken = scanToken();
        if (pkgToken.type != TOKEN_STRING) {
            char* text = getTokenText(pkgToken);
            setErrorF("KittyPort expects string, got '%s' (type: %s)", 
                      text, tokenTypeToString(pkgToken.type));
            return 1;
        }
        char pkgName[256];
        snprintf(pkgName, sizeof(pkgName), "%.*s", pkgToken.length - 2, pkgToken.start + 1);
        kitty_port(pkgName);
        return 1;
    }

    // ─── STALK_PACK ────────────────────────────────────────────
    if (t.type == TOKEN_STALK_PACK) {
        Token pathToken = scanToken();
        if (pathToken.type != TOKEN_STRING) {
            char* text = getTokenText(pathToken);
            setErrorF("Stalk_Pack expects string, got '%s' (type: %s)", 
                      text, tokenTypeToString(pathToken.type));
            return 1;
        }
        char filePath[LYNX_MAX_PATH];
        snprintf(filePath, sizeof(filePath), "%.*s", pathToken.length - 2, pathToken.start + 1);
        runFile(filePath, 0, NULL);
        return 1;
    }

    return 0;
}
