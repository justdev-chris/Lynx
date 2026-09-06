#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynx.h"
#include "platform.h"

// ─── GLOBAL DEFINITIONS ──────────────────────────────────────
char* lynx_error = NULL;
LynxError lynx_error_state = {0};

// ─── RETURN FLAG FOR EARLY EXIT ─────────────────────────────
int lynx_return_flag = 0;
double lynx_return_value = 0;

// ─── CONSTANTS ────────────────────────────────────────────────
#define MAX_VARS 1000
#define MAX_FUNCS 200
#define MAX_RECURSION 100
#define VAR_NAME_MAX 63

// ─── VARIABLE STORAGE ────────────────────────────────────────────
Variable den[MAX_VARS];
int varCount = 0;

// ─── FUNCTION STORAGE ────────────────────────────────────────────
typedef struct {
    char name[64];
    char params[10][64];
    int paramCount;
    char* body;
} FunctionDef;

FunctionDef functions[MAX_FUNCS];
int funcCount = 0;

// ─── RECURSION GUARD ────────────────────────────────────────────
static int recursionDepth = 0;

// ─── TEMP FILE PATH ─────────────────────────────────────────────
static char tempVarPath[LYNX_MAX_PATH];

// ─── ERROR STATE (storage only) ─────────────────────────────────
char* getError() {
    if (lynx_error_state.message) {
        return strdup(lynx_error_state.message);
    }
    return strdup("OK");
}

// ─── VARIABLE MANAGEMENT ────────────────────────────────────────
Variable* findVar(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < varCount; i++) {
        if (strcmp(den[i].name, name) == 0) return &den[i];
    }
    return NULL;
}

void setVar(const char* name, double val) {
    if (!name || strlen(name) == 0 || strlen(name) > VAR_NAME_MAX) {
        printf("🐾 ERROR: Invalid variable name\n");
        return;
    }
    
    Variable* v = findVar(name);
    if (v) {
        // Free previous string or array data
        if (v->type == VAR_ARRAY) {
            for (int i = 0; i < v->array_capacity; i++) {
                if (v->value.array[i]) {
                    if (v->value.array[i]->type == VAR_STRING && v->value.array[i]->value.strValue) {
                        free(v->value.array[i]->value.strValue);
                        v->value.array[i]->value.strValue = NULL;
                    }
                    free(v->value.array[i]);
                    v->value.array[i] = NULL;
                }
            }
            free(v->value.array);
            v->value.array = NULL;
        }
        if (v->type == VAR_STRING && v->value.strValue) {
            free(v->value.strValue);
            v->value.strValue = NULL;
        }
        v->type = VAR_NUMBER;
        v->value.numValue = val;
        v->array_length = 0;
        v->array_capacity = 0;
        v->value.array = NULL;
        return;
    }
    
    if (varCount < MAX_VARS) {
        strncpy(den[varCount].name, name, VAR_NAME_MAX);
        den[varCount].name[VAR_NAME_MAX] = '\0';
        den[varCount].type = VAR_NUMBER;
        den[varCount].value.numValue = val;
        den[varCount].value.strValue = NULL;
        den[varCount].value.array = NULL;
        den[varCount].array_length = 0;
        den[varCount].array_capacity = 0;
        varCount++;
    } else {
        printf("🐾 ERROR: Max variables (%d) exceeded\n", MAX_VARS);
    }
}

void setVarString(const char* name, const char* value) {
    if (!name || strlen(name) == 0 || strlen(name) > VAR_NAME_MAX) {
        printf("🐾 ERROR: setVarString called with invalid name\n");
        return;
    }
    // Allow empty string values
    if (!value) value = "";
    
    Variable* v = findVar(name);
    if (v) {
        // Free previous string or array data
        if (v->type == VAR_ARRAY) {
            for (int i = 0; i < v->array_capacity; i++) {
                if (v->value.array[i]) {
                    if (v->value.array[i]->type == VAR_STRING && v->value.array[i]->value.strValue) {
                        free(v->value.array[i]->value.strValue);
                        v->value.array[i]->value.strValue = NULL;
                    }
                    free(v->value.array[i]);
                    v->value.array[i] = NULL;
                }
            }
            free(v->value.array);
            v->value.array = NULL;
        }
        if (v->type == VAR_STRING && v->value.strValue) {
            free(v->value.strValue);
            v->value.strValue = NULL;
        }
        
        v->value.strValue = malloc(strlen(value) + 1);
        if (v->value.strValue) {
            strcpy(v->value.strValue, value);
            v->type = VAR_STRING;
        } else {
            printf("🐾 ERROR: Out of memory for string\n");
            v->type = VAR_STRING;
            v->value.strValue = NULL;
        }
        v->array_length = 0;
        v->array_capacity = 0;
        // FIXED: Removed v->value.array = NULL; - this was clobbering strValue
        return;
    }
    
    if (varCount < MAX_VARS) {
        strncpy(den[varCount].name, name, VAR_NAME_MAX);
        den[varCount].name[VAR_NAME_MAX] = '\0';
        den[varCount].type = VAR_STRING;
        den[varCount].value.strValue = malloc(strlen(value) + 1);
        if (den[varCount].value.strValue) {
            strcpy(den[varCount].value.strValue, value);
        } else {
            printf("🐾 ERROR: Out of memory for string\n");
            den[varCount].value.strValue = NULL;
        }
        den[varCount].value.array = NULL;
        den[varCount].array_length = 0;
        den[varCount].array_capacity = 0;
        varCount++;
    } else {
        printf("🐾 ERROR: Max variables (%d) exceeded\n", MAX_VARS);
    }
}

double getVar(const char* name) {
    if (!name) return 0;
    Variable* v = findVar(name);
    if (v && v->type == VAR_NUMBER) return v->value.numValue;
    return 0;
}

char* getVarString(const char* name) {
    if (!name) return "";
    Variable* v = findVar(name);
    if (v && v->type == VAR_STRING && v->value.strValue) {
        return v->value.strValue;
    }
    // Return empty string for missing or non-string variables
    return "";
}

void setArrayElement(const char* name, int index, double value) {
    if (!name || index < 0) return;
    
    Variable* v = findVar(name);
    if (!v) {
        if (varCount < MAX_VARS) {
            strncpy(den[varCount].name, name, VAR_NAME_MAX);
            den[varCount].name[VAR_NAME_MAX] = '\0';
            den[varCount].type = VAR_ARRAY;
            den[varCount].array_capacity = (index + 1) * 2;
            if (den[varCount].array_capacity < 8) den[varCount].array_capacity = 8;
            den[varCount].value.array = malloc(den[varCount].array_capacity * sizeof(Variable*));
            if (!den[varCount].value.array) {
                printf("🐾 ERROR: Out of memory for array\n");
                return;
            }
            for (int i = 0; i < den[varCount].array_capacity; i++) {
                den[varCount].value.array[i] = NULL;
            }
            v = &den[varCount];
            varCount++;
        } else {
            printf("🐾 ERROR: Max variables exceeded\n");
            return;
        }
    }
    
    if (v->type != VAR_ARRAY) {
        printf("🐾 ERROR: Variable is not an array\n");
        return;
    }
    
    if (index >= v->array_capacity) {
        int newCapacity = (index + 1) * 2;
        Variable** newArray = realloc(v->value.array, newCapacity * sizeof(Variable*));
        if (!newArray) {
            printf("🐾 ERROR: Out of memory expanding array\n");
            return;
        }
        for (int i = v->array_capacity; i < newCapacity; i++) {
            newArray[i] = NULL;
        }
        v->value.array = newArray;
        v->array_capacity = newCapacity;
    }
    
    if (!v->value.array[index]) {
        v->value.array[index] = malloc(sizeof(Variable));
        if (!v->value.array[index]) {
            printf("🐾 ERROR: Out of memory for array element\n");
            return;
        }
        v->value.array[index]->type = VAR_NUMBER;
        v->value.array[index]->value.numValue = 0;
        v->value.array[index]->value.strValue = NULL;
    }
    
    // Clean previous string if any
    if (v->value.array[index]->type == VAR_STRING && v->value.array[index]->value.strValue) {
        free(v->value.array[index]->value.strValue);
        v->value.array[index]->value.strValue = NULL;
    }
    
    v->value.array[index]->type = VAR_NUMBER;
    v->value.array[index]->value.numValue = value;
    if (index + 1 > v->array_length) v->array_length = index + 1;
}

double getArrayElement(const char* name, int index) {
    if (!name || index < 0) return 0;
    
    Variable* v = findVar(name);
    if (v && v->type == VAR_ARRAY && index < v->array_length && v->value.array[index]) {
        if (v->value.array[index]->type == VAR_NUMBER) {
            return v->value.array[index]->value.numValue;
        }
    }
    return 0;
}

int getArrayLength(const char* name) {
    if (!name) return 0;
    Variable* v = findVar(name);
    if (v && v->type == VAR_ARRAY) return v->array_length;
    return 0;
}

void setArrayStringElement(const char* name, int index, const char* value) {
    if (!name || index < 0) return;
    if (!value) value = "";
    
    Variable* v = findVar(name);
    if (!v) {
        if (varCount < MAX_VARS) {
            strncpy(den[varCount].name, name, VAR_NAME_MAX);
            den[varCount].name[VAR_NAME_MAX] = '\0';
            den[varCount].type = VAR_ARRAY;
            den[varCount].array_capacity = (index + 1) * 2;
            if (den[varCount].array_capacity < 8) den[varCount].array_capacity = 8;
            den[varCount].value.array = malloc(den[varCount].array_capacity * sizeof(Variable*));
            if (!den[varCount].value.array) {
                printf("🐾 ERROR: Out of memory for array\n");
                return;
            }
            for (int i = 0; i < den[varCount].array_capacity; i++) {
                den[varCount].value.array[i] = NULL;
            }
            v = &den[varCount];
            varCount++;
        } else {
            printf("🐾 ERROR: Max variables exceeded\n");
            return;
        }
    }
    
    if (v->type != VAR_ARRAY) {
        printf("🐾 ERROR: Variable is not an array\n");
        return;
    }
    
    if (index >= v->array_capacity) {
        int newCapacity = (index + 1) * 2;
        Variable** newArray = realloc(v->value.array, newCapacity * sizeof(Variable*));
        if (!newArray) {
            printf("🐾 ERROR: Out of memory expanding array\n");
            return;
        }
        for (int i = v->array_capacity; i < newCapacity; i++) {
            newArray[i] = NULL;
        }
        v->value.array = newArray;
        v->array_capacity = newCapacity;
    }
    
    if (!v->value.array[index]) {
        v->value.array[index] = malloc(sizeof(Variable));
        if (!v->value.array[index]) {
            printf("🐾 ERROR: Out of memory for array element\n");
            return;
        }
        v->value.array[index]->type = VAR_STRING;
        v->value.array[index]->value.numValue = 0;
        v->value.array[index]->value.strValue = NULL;
    }
    
    if (v->value.array[index]->type == VAR_STRING && v->value.array[index]->value.strValue) {
        free(v->value.array[index]->value.strValue);
        v->value.array[index]->value.strValue = NULL;
    }
    
    v->value.array[index]->type = VAR_STRING;
    v->value.array[index]->value.strValue = malloc(strlen(value) + 1);
    if (v->value.array[index]->value.strValue) {
        strcpy(v->value.array[index]->value.strValue, value);
    } else {
        printf("🐾 ERROR: Out of memory for string\n");
    }
    if (index + 1 > v->array_length) v->array_length = index + 1;
}

char* getArrayStringElement(const char* name, int index) {
    if (!name || index < 0) return "";
    
    Variable* v = findVar(name);
    if (v && v->type == VAR_ARRAY && index < v->array_length && v->value.array[index]) {
        if (v->value.array[index]->type == VAR_STRING && v->value.array[index]->value.strValue) {
            return v->value.array[index]->value.strValue;
        }
    }
    return "";
}

void pounce(const char* name) {
    if (!name) return;
    for (int i = 0; i < varCount; i++) {
        if (strcmp(den[i].name, name) == 0) {
            if (den[i].type == VAR_STRING && den[i].value.strValue) {
                free(den[i].value.strValue);
                den[i].value.strValue = NULL;
            } else if (den[i].type == VAR_ARRAY && den[i].value.array) {
                for (int j = 0; j < den[i].array_capacity; j++) {
                    if (den[i].value.array[j]) {
                        if (den[i].value.array[j]->type == VAR_STRING && den[i].value.array[j]->value.strValue) {
                            free(den[i].value.array[j]->value.strValue);
                        }
                        free(den[i].value.array[j]);
                    }
                }
                free(den[i].value.array);
                den[i].value.array = NULL;
            }
            // Shift remaining variables
            for (int j = i; j < varCount - 1; j++) {
                den[j] = den[j + 1];
            }
            varCount--;
            printf("🐾 Deleted variable: %s\n", name);
            return;
        }
    }
}

void hunt() {
    if (varCount == 0) {
        printf("🐾 No variables defined\n");
        return;
    }
    printf("🐾 Variables:\n");
    for (int i = 0; i < varCount; i++) {
        if (den[i].type == VAR_NUMBER) {
            printf("  %s = %.6g\n", den[i].name, den[i].value.numValue);
        } else if (den[i].type == VAR_STRING) {
            printf("  %s = \"%s\"\n", den[i].name, den[i].value.strValue ? den[i].value.strValue : "");
        } else if (den[i].type == VAR_ARRAY) {
            printf("  %s [ %d ] (array, %d elements)\n", den[i].name, den[i].array_capacity, den[i].array_length);
        }
    }
    printf("\n");
}

// ─── FUNCTIONS ──────────────────────────────────────────────────
void defineFunction(const char* name, const char** params, int paramCount, const char* body) {
    if (!name || !body) {
        printf("🐾 ERROR: Invalid function definition\n");
        return;
    }
    if (funcCount >= MAX_FUNCS) {
        printf("🐾 ERROR: Max functions (%d) exceeded\n", MAX_FUNCS);
        return;
    }
    if (strlen(name) > 63) {
        printf("🐾 ERROR: Function name too long\n");
        return;
    }
    if (paramCount > 10) {
        printf("🐾 ERROR: Too many parameters (max 10)\n");
        return;
    }
    
    strncpy(functions[funcCount].name, name, 63);
    functions[funcCount].name[63] = '\0';
    functions[funcCount].paramCount = paramCount;
    functions[funcCount].body = malloc(strlen(body) + 1);
    if (!functions[funcCount].body) {
        printf("🐾 ERROR: Out of memory for function body\n");
        return;
    }
    strcpy(functions[funcCount].body, body);
    
    for (int i = 0; i < paramCount; i++) {
        if (!params[i] || strlen(params[i]) > 63) {
            printf("🐾 ERROR: Invalid parameter name\n");
            free(functions[funcCount].body);
            functions[funcCount].body = NULL;
            return;
        }
        strncpy(functions[funcCount].params[i], params[i], 63);
        functions[funcCount].params[i][63] = '\0';
    }
    
    funcCount++;
    printf("🐾 Defined function: %s\n", name);
}

int callFunction(const char* name) {
    if (!name) return 0;
    if (recursionDepth >= MAX_RECURSION) {
        setError("Recursion depth exceeded", 0, 0);
        return 0;
    }
    recursionDepth++;
    
    for (int i = 0; i < funcCount; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            // Simple call - just run the body with current variables
            // (A more complete implementation would push a new scope)
            Scanner previous = scanner;
            initScanner(functions[i].body);
            while (peekToken().type != TOKEN_EOF) {
                parse_statement();
                if (lynx_error) {
                    fprintf(stderr, "🐾 %s\n", lynx_error);
                    clearError();
                    break;
                }
                // FIXED: Check return flag after each statement
                if (lynx_return_flag) {
                    lynx_return_flag = 0;
                    recursionDepth--;
                    return (int)lynx_return_value;
                }
            }
            scanner = previous;
            
            recursionDepth--;
            return 1;
        }
    }
    
    recursionDepth--;
    return 0;
}

// ─── CLEANUP ────────────────────────────────────────────────────
void cleanup_all() {
    for (int i = 0; i < varCount; i++) {
        if (den[i].type == VAR_STRING && den[i].value.strValue) {
            free(den[i].value.strValue);
            den[i].value.strValue = NULL;
        } else if (den[i].type == VAR_ARRAY && den[i].value.array) {
            for (int j = 0; j < den[i].array_capacity; j++) {
                if (den[i].value.array[j]) {
                    if (den[i].value.array[j]->type == VAR_STRING && den[i].value.array[j]->value.strValue) {
                        free(den[i].value.array[j]->value.strValue);
                    }
                    free(den[i].value.array[j]);
                }
            }
            free(den[i].value.array);
            den[i].value.array = NULL;
        }
    }
    varCount = 0;
    
    for (int i = 0; i < funcCount; i++) {
        if (functions[i].body) {
            free(functions[i].body);
            functions[i].body = NULL;
        }
    }
    funcCount = 0;
    
    if (lynx_error) {
        free(lynx_error);
        lynx_error = NULL;
    }
    if (lynx_error_state.message) {
        free(lynx_error_state.message);
        lynx_error_state.message = NULL;
    }
}

// ─── VARIABLE FILE PERSISTENCE ────────────────────────────────
void save_vars_to_temp() {
    #ifdef _WIN32
    const char* tempDir = getenv("TEMP");
    if (!tempDir) tempDir = "C:\\Temp";
    snprintf(tempVarPath, LYNX_MAX_PATH, "%s\\.lynx_vars.tmp", tempDir);
    #else
    snprintf(tempVarPath, LYNX_MAX_PATH, "/tmp/.lynx_vars.tmp");
    #endif
    
    FILE* f = fopen(tempVarPath, "w");
    if (!f) {
        printf("🐾 Warning: Could not save variables to temp file\n");
        return;
    }
    
    fprintf(f, "%d\n", varCount);
    for (int i = 0; i < varCount; i++) {
        fprintf(f, "%s|%d|", den[i].name, den[i].type);
        if (den[i].type == VAR_NUMBER) {
            fprintf(f, "%f\n", den[i].value.numValue);
        } else if (den[i].type == VAR_STRING && den[i].value.strValue) {
            // Note: this format is fragile if the string contains | or newlines.
            // For the current use case (preserve_vars for init) it is acceptable.
            fprintf(f, "%s\n", den[i].value.strValue);
        } else if (den[i].type == VAR_ARRAY) {
            fprintf(f, "%d\n", den[i].array_length);
            for (int j = 0; j < den[i].array_length; j++) {
                if (den[i].value.array[j]) {
                    fprintf(f, "%d|", j);
                    if (den[i].value.array[j]->type == VAR_NUMBER) {
                        fprintf(f, "num|%f\n", den[i].value.array[j]->value.numValue);
                    } else if (den[i].value.array[j]->type == VAR_STRING) {
                        fprintf(f, "str|%s\n", den[i].value.array[j]->value.strValue ? den[i].value.array[j]->value.strValue : "");
                    }
                }
            }
            fprintf(f, "ENDARRAY\n");
        } else {
            fprintf(f, "\n");
        }
    }
    fclose(f);
}

void load_vars_from_temp() {
    FILE* f = fopen(tempVarPath, "r");
    if (!f) {
        return;
    }
    
    // Clean up current variables
    for (int i = 0; i < varCount; i++) {
        if (den[i].type == VAR_STRING && den[i].value.strValue) {
            free(den[i].value.strValue);
            den[i].value.strValue = NULL;
        }
        if (den[i].type == VAR_ARRAY && den[i].value.array) {
            for (int j = 0; j < den[i].array_capacity; j++) {
                if (den[i].value.array[j]) {
                    if (den[i].value.array[j]->type == VAR_STRING && den[i].value.array[j]->value.strValue) {
                        free(den[i].value.array[j]->value.strValue);
                        den[i].value.array[j]->value.strValue = NULL;
                    }
                    free(den[i].value.array[j]);
                    den[i].value.array[j] = NULL;
                }
            }
            free(den[i].value.array);
            den[i].value.array = NULL;
        }
    }
    varCount = 0;
    
    char line[4096];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return;
    }
    int savedCount = atoi(line);
    if (savedCount > MAX_VARS) savedCount = MAX_VARS;
    
    for (int i = 0; i < savedCount; i++) {
        if (!fgets(line, sizeof(line), f)) break;
        
        char name[64];
        int type;
        if (sscanf(line, "%63[^|]|%d|", name, &type) != 2) {
            continue;
        }
        
        strncpy(den[varCount].name, name, VAR_NAME_MAX);
        den[varCount].name[VAR_NAME_MAX] = '\0';
        den[varCount].type = type;
        den[varCount].value.strValue = NULL;
        den[varCount].value.array = NULL;
        den[varCount].array_length = 0;
        den[varCount].array_capacity = 0;
        
        if (type == VAR_NUMBER) {
            double val = 0;
            // Skip past the two '|' separators
            char* p = strchr(line, '|');
            if (p) {
                p = strchr(p + 1, '|');
                if (p) {
                    val = atof(p + 1);
                }
            }
            den[varCount].value.numValue = val;
            varCount++;
        } else if (type == VAR_STRING) {
            char* val = strchr(line, '|');
            if (val) {
                val = strchr(val + 1, '|');
                if (val) {
                    val++;
                    size_t len = strlen(val);
                    if (len > 0 && val[len-1] == '\n') {
                        val[len-1] = '\0';
                        len--;
                    }
                    den[varCount].value.strValue = malloc(len + 1);
                    if (den[varCount].value.strValue) {
                        strcpy(den[varCount].value.strValue, val);
                    }
                    varCount++;
                }
            }
        } else if (type == VAR_ARRAY) {
            // Basic array restore (length only for now)
            varCount++;
        }
    }
    
    fclose(f);
    remove(tempVarPath);
}

void clear_temp_vars() {
    remove(tempVarPath);
}