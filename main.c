#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lynx.h"

// Unified help display
void show_help() {
    printf("\n🐾 LYNX COMMANDS:\n");
    printf("  Set x = 10         - Create a variable\n");
    printf("  Roar x             - Print a variable\n");
    printf("  Hunt               - List all variables\n");
    printf("  Stash \"file.lnx\"   - Save state\n");
    printf("  Stalk_Pack \"file\"  - Run a script\n");
    printf("  Help               - Show this menu\n\n");
}

void runFile(const char* path) {
    FILE* file = fopen(path, "rb");
    
    // Check local directory, then check std/ folder
    if (file == NULL) {
        char stdPath[512];
        snprintf(stdPath, sizeof(stdPath), "std/%s", path);
        file = fopen(stdPath, "rb");
    }

    if (file == NULL) {
        fprintf(stderr, "🐾 Lynx Error: Pack '%s' not found.\n", path);
        return;
    }

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char* buf = malloc(size + 1);
    fread(buf, 1, size, file);
    buf[size] = '\0';
    fclose(file);

    initScanner(buf);
    while (peekToken().type != TOKEN_EOF) {
        parse_statement();
    }
    free(buf);
}

int main(int argc, char* argv[]) {
    // MODE 1: Command Line Arguments
    if (argc == 2) {
        // Handle: lynx help
        if (_stricmp(argv[1], "help") == 0 || _stricmp(argv[1], "--help") == 0) {
            show_help();
        } 
        // Handle: lynx script.lnx
        else {
            runFile(argv[1]);
        }
        return 0;
    } 

    // MODE 2: REPL (Interactive)
    char line[1024];
    printf("Lynx Engine v1.3 | Type 'Help' for info\n");

    while (1) {
        printf("lynx > ");
        if (!fgets(line, sizeof(line), stdin)) break;

        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        // REPL Shortcut: If user types "help"
        if (_stricmp(line, "help") == 0) {
            show_help();
            continue;
        }

        // REPL Shortcut: If user types "anything.lnx", run it
        if (strstr(line, ".lnx") != NULL && strchr(line, ' ') == NULL) {
            runFile(line);
            continue;
        }

        initScanner(line);
        parse_statement();
    }
    
    return 0;
}
