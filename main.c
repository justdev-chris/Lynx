#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif
#include "lynx.h"

int main() {
    char line[1024];
    while (1) {
        // Now works on Windows and Linux!
        if (isatty(0)) printf("lynx > "); 
        
        if (!fgets(line, sizeof(line), stdin)) break;
        initScanner(line);
        parse_statement();
    }
    return 0;
}
