#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "lynx.h"

typedef struct {
    char name[64];
    HINSTANCE handle;
} LoadedLib;

LoadedLib loaded_libs[32];
int lib_count = 0;

void load_lib(const char* lib_name) {
    // Check if already loaded
    for (int i = 0; i < lib_count; i++) {
        if (strcmp(loaded_libs[i].name, lib_name) == 0) {
            printf("🐾 Library %s already loaded\n", lib_name);
            return;
        }
    }
    
    char path[MAX_PATH];
    sprintf(path, ".\\lib\\%s.dll", lib_name);
    
    HINSTANCE handle = LoadLibrary(path);
    if (handle) {
        strcpy(loaded_libs[lib_count].name, lib_name);
        loaded_libs[lib_count++].handle = handle;
        printf("🐾 Loaded library: %s\n", lib_name);
    } else {
        printf("🐾 Failed to load %s.dll from ./lib/\n", lib_name);
    }
}

void unload_all_libs() {
    for (int i = 0; i < lib_count; i++) {
        FreeLibrary(loaded_libs[i].handle);
    }
    lib_count = 0;
}