#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynx.h"
#include "platform.h"

typedef struct {
    char name[64];
    HINSTANCE handle;
} LoadedLib;

LoadedLib loaded_libs[32];
int lib_count = 0;

void load_lib(const char* lib_name) {
    if (!lib_name || strlen(lib_name) > 63 || strlen(lib_name) == 0) {
        printf("🐾 Error: Invalid library name\n");
        return;
    }
    
    // Check if already loaded
    for (int i = 0; i < lib_count; i++) {
        if (strcmp(loaded_libs[i].name, lib_name) == 0) {
            printf("🐾 Library %s already loaded\n", lib_name);
            return;
        }
    }
    
    // Check array bounds
    if (lib_count >= 32) {
        printf("🐾 Error: Maximum libraries (%d) already loaded\n", 32);
        return;
    }
    
    char path[LYNX_MAX_PATH];
    snprintf(path, LYNX_MAX_PATH, ".\\lib\\%s.dll", lib_name);
    
    HINSTANCE handle = LoadLibrary(path);
    if (handle) {
        strncpy(loaded_libs[lib_count].name, lib_name, 63);
        loaded_libs[lib_count].name[63] = '\0';
        loaded_libs[lib_count].handle = handle;
        lib_count++;
        printf("🐾 Loaded library: %s\n", lib_name);
    } else {
        printf("🐾 Failed to load %s.dll from ./lib/\n", lib_name);
    }
}

void unload_all_libs() {
    for (int i = 0; i < lib_count; i++) {
        if (loaded_libs[i].handle) {
            FreeLibrary(loaded_libs[i].handle);
            loaded_libs[i].handle = NULL;
        }
    }
    lib_count = 0;
}
