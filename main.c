#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define strdup _strdup
#else
#include <unistd.h>
#endif

#include "lynx.h"

// The current source being executed (for nesting)
const char* current_source_pointer = NULL;

void runFile(const char* path) {
    FILE* file = fopen(path, "rb");
    
    // Search Path: If not found in root, try std/
    if (file == NULL) {
        char stdPath[512];
        snprintf(stdPath, sizeof(stdPath), "std/%s", path);
        file = fopen(stdPath, "rb");
    }

    if (file == NULL) {
        fprintf(stderr, "🐾 Lynx Error: Could not find pack '%s'\n", path);
        return;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        fclose(file);
        return;
    }
    fread(buffer, sizeof(char), fileSize, file);
    buffer[fileSize] = '\0';
    fclose(file);

    // Save old state to allow nested Stalk_Pack calls
    const char* previousSource = current_source_pointer;
    initScanner(buffer);
    
    while (peekToken().type != TOKEN_EOF) {
        parse_statement();
    }

    free(buffer);
    current_source_pointer = previousSource;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        // Run as a compiler/interpreter for a file
        runFile(argv[1]);
    } else {
        // Run as a REPL
        char line[1024];
        printf("Lynx Engine v1.2\n");
        while (1) {
            if (isatty(0)) printf("lynx > ");
            if (!fgets(line, sizeof(line), stdin)) break;
            initScanner(line);
            parse_statement();
        }
    }
    return 0;
}
