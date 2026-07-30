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

static int is_command_token(LynxTokenType type) {
    return type == TOKEN_SET || type == TOKEN_IF || type == TOKEN_FOR ||
           type == TOKEN_WHILE || type == TOKEN_FUNC || type == TOKEN_ROAR ||
           type == TOKEN_HUNT || type == TOKEN_POUNCE || type == TOKEN_STALK_PACK ||
           type == TOKEN_LOAD_LIB || type == TOKEN_KITTY_WRITE_FILE || 
           type == TOKEN_KITTY_READ_FILE || type == TOKEN_PAW || 
           type == TOKEN_KITTY_FILE_EXISTS || type == TOKEN_RUN ||
           type == TOKEN_GETENV || type == TOKEN_EXPORT || type == TOKEN_KITTY_PORT ||
           type == TOKEN_KITTY_REMOVE_FILE || type == TOKEN_KITTY_LIST_FILES ||
           type == TOKEN_ELSE || type == TOKEN_RETURN || type == TOKEN_BREAK ||
           type == TOKEN_CONTINUE || type == TOKEN_KITTY_READ_DIR ||
           type == TOKEN_GET_ERROR || type == TOKEN_STRING_SPLIT ||
           type == TOKEN_STRING_CONTAINS || type == TOKEN_STRING_REPLACE ||
           type == TOKEN_TRIM || type == TOKEN_LEN || type == TOKEN_ARGV;
}

int pawcom_parse_statement(Token t) {
    if (t.type == TOKEN_HUNT) { hunt(); return 1; }

    // ─── ROAR ──────────────────────────────────────────────────
    if (t.type == TOKEN_ROAR) {
        char result[4096] = "";
        
        while (1) {
            Token next = peekToken();
            
            // If next token is a command, stop (don't consume it)
            if (is_command_token(next.type)) {
                break;
            }
            
            Token val = scanToken();
            
            if (val.type == TOKEN_EOF || val.type == TOKEN_ERROR) {
                break;
            }
            
            if (val.type == TOKEN_STRING) {
                char str[4096];
                if (val.length > 1) {
                    snprintf(str, sizeof(str), "%.*s", val.length - 2, val.start + 1);
                } else {
                    str[0] = '\0';
                }
                strncat(result, str, sizeof(result) - strlen(result) - 1);
            } else if (val.type == TOKEN_IDENTIFIER) {
                char name[64];
                snprintf(name, sizeof(name), "%.*s", val.length, val.start);
                char* s = getVarString(name);
                if (s && strlen(s) > 0) {
                    strncat(result, s, sizeof(result) - strlen(result) - 1);
                } else {
                    double num = getVar(name);
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
                continue;
            }
            
            Token after = peekToken();
            if (after.type == TOKEN_PLUS) {
                scanToken();
                continue;
            } else {
                break;
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

        // Check if it's a string expression
        Token first = peekToken();
        if (first.type == TOKEN_STRING || first.type == TOKEN_IDENTIFIER) {
            char result[4096] = "";
            
            while (1) {
                Token next = peekToken();
                
                // If next token is a command, stop
                if (is_command_token(next.type)) {
                    break;
                }
                
                Token val = scanToken();
                
                if (val.type == TOKEN_EOF || val.type == TOKEN_ERROR) {
                    break;
                }
                
                if (val.type == TOKEN_STRING) {
                    char str[4096];
                    if (val.length > 1) {
                        snprintf(str, sizeof(str), "%.*s", val.length - 2, val.start + 1);
                    } else {
                        str[0] = '\0';
                    }
                    strncat(result, str, sizeof(result) - strlen(result) - 1);
                } else if (val.type == TOKEN_IDENTIFIER) {
                    char name[64];
                    snprintf(name, sizeof(name), "%.*s", val.length, val.start);
                    char* s = getVarString(name);
                    if (s && strlen(s) > 0) {
                        strncat(result, s, sizeof(result) - strlen(result) - 1);
                    } else {
                        double num = getVar(name);
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
                        setErrorF("Set expects string, identifier, or number, got '%s' (type: %s)",
                                  text, tokenTypeToString(val.type));
                        printf("%s\n", lynx_error);
                        clearError();
                        return 1;
                    }
                    continue;
                }
            }
            setVarString(varName, result);
        } else {
            // Numeric expression
            double value = parse_expression();
            if (lynx_error) return 1;
            setVar(varName, value);
        }
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

    // ─── KITTY_WRITE_FILE ─────────────────────────────────────
    if (t.type == TOKEN_KITTY_WRITE_FILE) {
        Token pathToken = scanToken();
        Token contentToken = scanToken();
        
        if (pathToken.type != TOKEN_STRING && pathToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyWriteFile expects path (string or variable)");
            return 1;
        }
        if (contentToken.type != TOKEN_STRING && contentToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyWriteFile expects content (string or variable)");
            return 1;
        }
        
        char path[LYNX_MAX_PATH];
        char content[MAX_STRING];
        
        if (pathToken.type == TOKEN_STRING) {
            snprintf(path, sizeof(path), "%.*s", pathToken.length - 2, pathToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", pathToken.length, pathToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(path, sizeof(path), "%s", val);
            } else {
                setErrorF("KittyWriteFile: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        if (contentToken.type == TOKEN_STRING) {
            snprintf(content, sizeof(content), "%.*s", contentToken.length - 2, contentToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", contentToken.length, contentToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(content, sizeof(content), "%s", val);
            } else {
                setErrorF("KittyWriteFile: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        FILE* f = fopen(path, "w");
        if (f) {
            fwrite(content, 1, strlen(content), f);
            fclose(f);
        } else {
            setErrorF("KittyWriteFile: Could not open '%s' for writing", path);
        }
        return 1;
    }

    // ─── KITTY_READ_FILE ──────────────────────────────────────
    if (t.type == TOKEN_KITTY_READ_FILE) {
        Token pathToken = scanToken();
        if (pathToken.type != TOKEN_STRING && pathToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReadFile expects string or variable");
            return 1;
        }
        
        char path[LYNX_MAX_PATH];
        if (pathToken.type == TOKEN_STRING) {
            snprintf(path, sizeof(path), "%.*s", pathToken.length - 2, pathToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", pathToken.length, pathToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(path, sizeof(path), "%s", val);
            } else {
                setErrorF("KittyReadFile: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        FILE* f = fopen(path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            rewind(f);
            char* buf = malloc(size + 1);
            if (buf) {
                fread(buf, 1, size, f);
                buf[size] = '\0';
                fclose(f);
                setVarString("__file_content", buf);
                free(buf);
            } else {
                fclose(f);
                setErrorF("Out of memory reading file");
            }
        } else {
            setErrorF("KittyReadFile: File '%s' not found", path);
        }
        return 1;
    }

    // ─── PAW ──────────────────────────────────────────────────
    if (t.type == TOKEN_PAW) {
        Token pathToken = scanToken();
        if (pathToken.type != TOKEN_STRING && pathToken.type != TOKEN_IDENTIFIER) {
            setErrorF("Paw expects string or variable");
            return 1;
        }
        
        char path[LYNX_MAX_PATH];
        if (pathToken.type == TOKEN_STRING) {
            snprintf(path, sizeof(path), "%.*s", pathToken.length - 2, pathToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", pathToken.length, pathToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(path, sizeof(path), "%s", val);
            } else {
                setErrorF("Paw: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        #ifdef _WIN32
        if (_mkdir(path) != 0 && errno != EEXIST) {
            setErrorF("Paw: Could not create directory '%s'", path);
        }
        #else
        if (mkdir(path, 0777) != 0 && errno != EEXIST) {
            setErrorF("Paw: Could not create directory '%s'", path);
        }
        #endif
        return 1;
    }

    // ─── KITTY_FILE_EXISTS ────────────────────────────────────
    if (t.type == TOKEN_KITTY_FILE_EXISTS) {
        Token pathToken = scanToken();
        if (pathToken.type != TOKEN_STRING && pathToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyFileExists expects string or variable");
            return 1;
        }
        
        char path[LYNX_MAX_PATH];
        if (pathToken.type == TOKEN_STRING) {
            snprintf(path, sizeof(path), "%.*s", pathToken.length - 2, pathToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", pathToken.length, pathToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(path, sizeof(path), "%s", val);
            } else {
                setErrorF("KittyFileExists: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        FILE* f = fopen(path, "r");
        setVar("__result", f ? 1.0 : 0.0);
        if (f) fclose(f);
        return 1;
    }

    // ─── RUN ──────────────────────────────────────────────────
    if (t.type == TOKEN_RUN) {
        Token cmdToken = scanToken();
        if (cmdToken.type != TOKEN_STRING) {
            setErrorF("Run expects string");
            return 1;
        }
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%.*s", cmdToken.length - 2, cmdToken.start + 1);
        int result = system(cmd);
        if (result != 0) {
            setErrorF("Run: Command failed with exit code %d", result);
        }
        return 1;
    }

    // ─── KITTY_REMOVE_FILE ────────────────────────────────────
    if (t.type == TOKEN_KITTY_REMOVE_FILE) {
        Token pathToken = scanToken();
        if (pathToken.type != TOKEN_STRING && pathToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyRemoveFile expects string or variable");
            return 1;
        }
        
        char path[LYNX_MAX_PATH];
        if (pathToken.type == TOKEN_STRING) {
            snprintf(path, sizeof(path), "%.*s", pathToken.length - 2, pathToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", pathToken.length, pathToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(path, sizeof(path), "%s", val);
            } else {
                setErrorF("KittyRemoveFile: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        if (remove(path) != 0) {
            setErrorF("KittyRemoveFile: Could not remove '%s'", path);
        }
        return 1;
    }

    // ─── GETENV ──────────────────────────────────────────────────
    if (t.type == TOKEN_GETENV) {
        Token nameToken = scanToken();
        if (nameToken.type != TOKEN_STRING) {
            setErrorF("getenv expects string");
            return 1;
        }
        char name[256];
        snprintf(name, sizeof(name), "%.*s", nameToken.length - 2, nameToken.start + 1);
        const char* value = getenv(name);
        if (value) {
            setVarString("__result", value);
        } else {
            setVarString("__result", "");
        }
        return 1;
    }

    // ─── LEN ──────────────────────────────────────────────────
    if (t.type == TOKEN_LEN) {
        Token strToken = scanToken();
        if (strToken.type != TOKEN_STRING && strToken.type != TOKEN_IDENTIFIER) {
            setErrorF("Len expects string or variable");
            return 1;
        }
        
        char str[MAX_STRING];
        if (strToken.type == TOKEN_STRING) {
            snprintf(str, sizeof(str), "%.*s", strToken.length - 2, strToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", strToken.length, strToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(str, sizeof(str), "%s", val);
            } else {
                str[0] = '\0';
            }
        }
        
        setVar("__result", (double)strlen(str));
        return 1;
    }

    // ─── TRIM ──────────────────────────────────────────────────
    if (t.type == TOKEN_TRIM) {
        Token strToken = scanToken();
        if (strToken.type != TOKEN_STRING && strToken.type != TOKEN_IDENTIFIER) {
            setErrorF("Trim expects string or variable");
            return 1;
        }
        
        char str[MAX_STRING];
        if (strToken.type == TOKEN_STRING) {
            snprintf(str, sizeof(str), "%.*s", strToken.length - 2, strToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", strToken.length, strToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(str, sizeof(str), "%s", val);
            } else {
                str[0] = '\0';
            }
        }
        
        char* trimmed = str_trim_copy(str);
        setVarString("__result", trimmed);
        free(trimmed);
        return 1;
    }

    // ─── KITTY_SPLIT_STRING ──────────────────────────────────
    if (t.type == TOKEN_STRING_SPLIT) {
        Token strToken = scanToken();
        Token delimToken = scanToken();
        
        if (strToken.type != TOKEN_STRING && strToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittySplitString expects string or variable");
            return 1;
        }
        if (delimToken.type != TOKEN_STRING && delimToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittySplitString expects string or variable");
            return 1;
        }
        
        char str[MAX_STRING];
        char delim[256];
        
        if (strToken.type == TOKEN_STRING) {
            snprintf(str, sizeof(str), "%.*s", strToken.length - 2, strToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", strToken.length, strToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(str, sizeof(str), "%s", val);
            } else {
                setErrorF("KittySplitString: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        if (delimToken.type == TOKEN_STRING) {
            snprintf(delim, sizeof(delim), "%.*s", delimToken.length - 2, delimToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", delimToken.length, delimToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(delim, sizeof(delim), "%s", val);
            } else {
                setErrorF("KittySplitString: delimiter variable '%s' is not a string", name);
                return 1;
            }
        }
        
        int count = 0;
        char** result = split_string(str, delim, &count);
        if (result) {
            for (int i = 0; i < count; i++) {
                setArrayStringElement("__result", i, result[i]);
                free(result[i]);
            }
            free(result);
        }
        setVar("__result_count", (double)count);
        return 1;
    }

    // ─── KITTY_CONTAINS ──────────────────────────────────────
    if (t.type == TOKEN_STRING_CONTAINS) {
        Token hayToken = scanToken();
        Token needleToken = scanToken();
        
        if (hayToken.type != TOKEN_STRING && hayToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyCheckIfStringContains expects string or variable");
            return 1;
        }
        if (needleToken.type != TOKEN_STRING && needleToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyCheckIfStringContains expects string or variable");
            return 1;
        }
        
        char hay[MAX_STRING];
        char needle[MAX_STRING];
        
        if (hayToken.type == TOKEN_STRING) {
            snprintf(hay, sizeof(hay), "%.*s", hayToken.length - 2, hayToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", hayToken.length, hayToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(hay, sizeof(hay), "%s", val);
            } else {
                setErrorF("KittyCheckIfStringContains: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        if (needleToken.type == TOKEN_STRING) {
            snprintf(needle, sizeof(needle), "%.*s", needleToken.length - 2, needleToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", needleToken.length, needleToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(needle, sizeof(needle), "%s", val);
            } else {
                setErrorF("KittyCheckIfStringContains: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        setVar("__result", str_contains(hay, needle) ? 1.0 : 0.0);
        return 1;
    }

    // ─── KITTY_REPLACE_STRING ──────────────────────────────
    if (t.type == TOKEN_STRING_REPLACE) {
        Token srcToken = scanToken();
        Token oldToken = scanToken();
        Token newToken = scanToken();
        
        if (srcToken.type != TOKEN_STRING && srcToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return 1;
        }
        if (oldToken.type != TOKEN_STRING && oldToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return 1;
        }
        if (newToken.type != TOKEN_STRING && newToken.type != TOKEN_IDENTIFIER) {
            setErrorF("KittyReplaceString expects string or variable");
            return 1;
        }
        
        char src[MAX_STRING];
        char oldStr[MAX_STRING];
        char newStr[MAX_STRING];
        
        if (srcToken.type == TOKEN_STRING) {
            snprintf(src, sizeof(src), "%.*s", srcToken.length - 2, srcToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", srcToken.length, srcToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(src, sizeof(src), "%s", val);
            } else {
                setErrorF("KittyReplaceString: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        if (oldToken.type == TOKEN_STRING) {
            snprintf(oldStr, sizeof(oldStr), "%.*s", oldToken.length - 2, oldToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", oldToken.length, oldToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(oldStr, sizeof(oldStr), "%s", val);
            } else {
                setErrorF("KittyReplaceString: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        if (newToken.type == TOKEN_STRING) {
            snprintf(newStr, sizeof(newStr), "%.*s", newToken.length - 2, newToken.start + 1);
        } else {
            char name[64];
            snprintf(name, sizeof(name), "%.*s", newToken.length, newToken.start);
            char* val = getVarString(name);
            if (val) {
                snprintf(newStr, sizeof(newStr), "%s", val);
            } else {
                setErrorF("KittyReplaceString: variable '%s' is not a string", name);
                return 1;
            }
        }
        
        char* result = str_replace(src, oldStr, newStr);
        setVarString("__result", result);
        return 1;
    }

    return 0;
}
