#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif
#include "lynx.h"

void runFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "🐾 Lynx Error: Could not find pack '%s'\n", path);
        return;
    }
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    fread(buffer, sizeof(char), fileSize, file);
    buffer[fileSize] = '\0';
    fclose(file);

    initScanner(buffer);
    while (peekToken().type != TOKEN_EOF) {
        parse_statement();
    }
    free(buffer);
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        runFile(argv[1]);
    } else {
        char line[1024];
        printf("Lynx Engine v1.1 - Multi-Module Mode\n");
        while (1) {
            if (isatty(0)) printf("lynx > ");
            if (!fgets(line, sizeof(line), stdin)) break;
            initScanner(line);
            parse_statement();
        }
    }
    return 0;
}
