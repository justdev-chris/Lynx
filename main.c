#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>
#include "lynx.h"

#pragma comment(lib, "urlmon.lib")

#define LYNX_VERSION "v1.3"

void show_help() {
    printf("\n🐾 LYNX COMMANDS:\n");
    printf("  Set x = 10         - Create/Update variable\n");
    printf("  Roar x             - Print value\n");
    printf("  Hunt               - Show sorted Den contents\n");
    printf("  Stalk_Pack \"file\"  - Run a .lnx script\n");
    printf("  Help               - Show this menu\n");
    printf("  Exit               - Close Lynx\n");
    printf("  --version          - Show version\n");
    printf("  --update           - Fetch newest LynxInstaller\n\n");
}

void runFile(const char* path) {
    char cleanPath[MAX_PATH];
    int j = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] != '\"' && path[i] != '\r' && path[i] != '\n') 
            cleanPath[j++] = path[i];
    }
    cleanPath[j] = '\0';

    // Try local first, then AppData std
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

        initScanner(buf);
        // Correct loop: check before parsing
        while (peekToken().type != TOKEN_EOF) {
            parse_statement(); 
        }
        free(buf);
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001); // UTF-8 fix

    if (argc >= 2) {
        // CLI Commands
        if (_stricmp(argv[1], "help") == 0 || _stricmp(argv[1], "--help") == 0) {
            show_help();
        } 
        else if (_stricmp(argv[1], "--version") == 0) {
            printf("Lynx Engine %s\n", LYNX_VERSION);
        }
        else if (_stricmp(argv[1], "--update") == 0) {
            printf("🔄 Preparing update sequence...\n");
            char tempInstaller[MAX_PATH];
            sprintf(tempInstaller, "%s\\LynxInstaller.exe", getenv("TEMP"));

            const char* url = "https://github.com/justdev-chris/Lynx/releases/latest/download/LynxInstaller.exe";
            
            printf("📥 Downloading newest LynxInstaller...\n");
            if (S_OK == URLDownloadToFileA(NULL, url, tempInstaller, 0, NULL)) {
                printf("🚀 Launching Installer. Closing session...\n");
                ShellExecuteA(NULL, "open", tempInstaller, NULL, NULL, SW_SHOWNORMAL);
                exit(0); 
            } else {
                printf("❌ Update failed: Could not reach GitHub.\n");
            }
        }
        else {
            runFile(argv[1]);
        }
        return 0;
    }

    // REPL Mode
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
            printf("🐾 Leaving the den...\n");
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
