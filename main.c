#include <stdio.h>
#include "lynx.h"

int main() {
    char line[1024];
    printf("Lynx Engine v1.0 - Ready to Hunt\n");
    while (1) {
        printf("lynx > ");
        if (!fgets(line, sizeof(line), stdin)) break;
        initScanner(line);
        parse_statement();
    }
    return 0;
}
