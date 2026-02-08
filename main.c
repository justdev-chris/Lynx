#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        char stdPath[512];
        snprintf(stdPath, sizeof(stdPath), "std/%s", path);
        file = fopen(stdPath, "rb");
    }
    if (!file) {
        fprintf(stderr, "🐾 Error: Pack '%s' not found.\n", path);
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
    while (peekToken().type != TOKEN_EOF) parse_statement();
    free(buf);
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        runFile(argv[1]);
    } else {
        char line[1024];
        printf("Lynx v1.3 \n");
        while (1) {
            if (isatty(0)) printf("lynx > ");
            if (!fgets(line, sizeof(line), stdin)) break;
            initScanner(line);
            parse_statement();
        }
    }
    return 0;
}
