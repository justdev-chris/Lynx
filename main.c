#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>
#include "lynx.h"

#pragma comment(lib, "urlmon.lib")

#define LYNX_VERSION "v1.3.2"

// Access the global scanner defined in scanner.c
extern Scanner scanner; 

void show_help() {
    printf("\n🐾 LYNX COMMANDS:\n");
    printf("  Set x = 10         - Create/Update variable\n");
    printf("  Roar x             - Print value\n");
    printf("  Hunt               - Show Den contents\n");
    printf("  Pounce x           - Delete variable x\n");
    printf("  If x > 5 { ... }   - Conditional execution\n");
    printf("  Stalk_Pack \"file\"  - Run a .lnx script\n");
    printf("  Help               - Show this menu\n");
    printf("  Exit               - Close Lynx\n");
    printf("  --version          - Show version\n");
    printf("  --update           - Fetch newest Lynx\n\n");
    printf("  filename.lnx       - Another method to run a .lnx script\n\n");
}

void runFile(const char* path) {
    char cleanPath[MAX_PATH];
    
    // Clean quotes from path
    if (path[0] == '"') {
        int len = strlen(path) - 2;
        strncpy(cleanPath, path + 1, len);
        cleanPath[len] = '\0';
    } else {
        strcpy(cleanPath, path);
    }

    FILE* file = fopen(cleanPath, "rb");
    if (!file) {
        char stdPath[MAX_PATH];
        sprintf(stdPath, "%s\\LynxLang\\std\\%s", getenv("APPDATA"), cleanPath);
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
    if (buf) {
        fread(buf, 1, size, file);
        buf[size] = '\0';
        fclose(file);

        // Save current scanner state
        Scanner previousScanner = scanner; 
        
        initScanner(buf);
        while (peekToken().type != TOKEN_EOF) {
            parse_statement(); 
        }

        // Restore state
        scanner = previousScanner;

        free(buf);
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001); // UTF-8 fix for the 🐾

    if (argc >= 2) {
        if (_stricmp(argv[1], "help") == 0 || _stricmp(argv[1], "--help") == 0) {
            show_help();
        } 
        else if (_stricmp(argv[1], "--version") == 0) {
            printf("Lynx Engine %s\n", LYNX_VERSION);
        }
        else if (_stricmp(argv[1], "--update") == 0) {
            printf("🔄 Preparing update...\n");
            char tempInstaller[MAX_PATH];
            sprintf(tempInstaller, "%s\\LynxInstaller.exe", getenv("TEMP"));
            const char* url = "https://github.com/justdev-chris/Lynx/releases/latest/download/LynxInstaller.exe";
            
            if (S_OK == URLDownloadToFileA(NULL, url, tempInstaller, 0, NULL)) {
                ShellExecuteA(NULL, "open", tempInstaller, NULL, NULL, SW_SHOWNORMAL);
                exit(0); 
            } else {
                printf("❌ Update failed.\n");
            }
        }
        else {
            runFile(argv[1]);
        }
        return 0;
    }

    char line[1024];
    printf("Lynx Engine %s | Type 'Help' for info\n", LYNX_VERSION);
    while (1) {
        printf("lynx > ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (_stricmp(line, "help") == 0) {
            show_help();
        } else if (_stricmp(line, "exit") == 0) {
            break;
        } else if (strstr(line, ".lnx") != NULL) {
            runFile(line);
        } else {
            initScanner(line);
            parse_statement();
        }
    }
    return 0;
}