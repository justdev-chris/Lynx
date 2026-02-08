#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "lynx.h"

void show_help() {
    printf("\n🐾 LYNX COMMANDS:\n");
    printf("  Set x = 10         - Create/Update variable\n");
    printf("  Roar x             - Print value\n");
    printf("  Hunt               - Show sorted Den contents\n");
    printf("  Stalk_Pack \"file\"  - Run a .lnx script\n");
    printf("  Help               - Show this menu\n\n");
}

void runFile(const char* path) {
    char cleanPath[MAX_PATH];
    int j = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] != '\"' && path[i] != '\r' && path[i] != '\n') 
            cleanPath[j++] = path[i];
    }
    cleanPath[j] = '\0';

    FILE* file = fopen(cleanPath, "rb");
    if (!file) {
        char stdPath[MAX_PATH];
        snprintf(stdPath, sizeof(stdPath), "std/%s", cleanPath);
        file = fopen(stdPath, "rb");
    }

    if (!file) {
        fprintf(stderr, "🐾 Lynx Error: Pack '%s' not found.\n", cleanPath);
        return;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* buf = malloc(size + 1);
    fread(buf, 1, size, file);
    buf[size] = '\0';
    fclose(file);

    initScanner(buf);
    while (scanToken().type != TOKEN_EOF) {
        // Your existing parser logic calls
        parse_statement(); 
    }
    free(buf);
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001); // Fixes the ≡ƒÉ╛ symbols

    if (argc == 2) {
        if (_stricmp(argv[1], "help") == 0) show_help();
        else runFile(argv[1]);
        return 0;
    }

    char line[1024];
    printf("Lynx Engine v1.3 | Type 'Help' for info\n");
    while (1) {
        printf("lynx > ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (_stricmp(line, "help") == 0) {
            show_help();
        } else if (strstr(line, ".lnx") != NULL) {
            runFile(line);
        } else {
            initScanner(line);
            parse_statement();
        }
    }
    return 0;
}
