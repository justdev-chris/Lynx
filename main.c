#include <stdio.h>
#include "lynx.h"

int main() {
    char line[1024];
    while (1) {
        if (isatty(0)) printf("lynx > "); // Only print prompt if interactive
        if (!fgets(line, sizeof(line), stdin)) break;
        initScanner(line);
        parse_statement();
    }
    return 0;
}
