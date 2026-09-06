#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "platform.h"
#include <errno.h>
#include "lynx.h"

// Case-insensitive string compare for cross-platform
#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

#define LYNX_VERSION "v1.4.0"

extern Scanner scanner;
extern char* lynx_error;
extern LynxError lynx_error_state;

// ─── GLOBAL TRY/CATCH STATE ──────────────────────────────────
TryState try_state = {0};

void show_help() {
    printf("\n🐾 LYNX %s COMMANDS:\n", LYNX_VERSION);
    printf("\n  init               - Create new Lynx project\n");
    printf("  add <pkg>          - Add dependency to lynx.toml\n");
    printf("  install            - Install all dependencies\n");
    printf("  remove <pkg>       - Remove a package\n");
    printf("  search <term>      - Search registry for packages\n");
    printf("  update             - Update all packages to latest\n");
    printf("  publish            - Pack project for registry\n");
    printf("  build              - Run src/main.lnx\n");
    printf("  run <file.lnx>     - Run script\n");
    printf("  fmt <file.lnx>     - Auto-format Lynx file\n");
    printf("  check <file.lnx>   - Check syntax without executing\n");
    printf("  --version          - Show version\n");
    printf("  --update           - Self-update\n");
    printf("  help               - Show this menu\n\n");
}

void runFile(const char* path, int argc, char** argv) {
    (void)argc;  // currently unused
    (void)argv;  // currently unused

    char cleanPath[LYNX_MAX_PATH];
    if (path[0] == '"') {
        int len = (int)strlen(path) - 2;
        if (len < 0) len = 0;
        strncpy(cleanPath, path + 1, len);
        cleanPath[len] = '\0';
    } else {
        strncpy(cleanPath, path, LYNX_MAX_PATH - 1);
        cleanPath[LYNX_MAX_PATH - 1] = '\0';
    }

    FILE* file = NULL;
    char fullPath[LYNX_MAX_PATH] = {0};
    
    file = fopen(cleanPath, "rb");
    
    if (!file) {
        char exePath[LYNX_MAX_PATH];
        #ifdef _WIN32
        GetModuleFileNameA(NULL, exePath, LYNX_MAX_PATH);
        #else
        ssize_t len = readlink("/proc/self/exe", exePath, LYNX_MAX_PATH - 1);
        if (len != -1) exePath[len] = '\0';
        else exePath[0] = '\0';
        #endif
        char* lastSlash = strrchr(exePath, PATH_SEP);
        if (lastSlash) {
            *lastSlash = '\0';
            snprintf(fullPath, LYNX_MAX_PATH, "%s%c%s", exePath, PATH_SEP, cleanPath);
            file = fopen(fullPath, "rb");
        }
    }
    
    if (!file) {
        char stdPath[LYNX_MAX_PATH];
        const char* appdata = getenv("APPDATA");
        if (!appdata) appdata = getenv("HOME");
        if (appdata) {
            snprintf(stdPath, LYNX_MAX_PATH, "%s%cLynxLang%cstd%c%s", appdata, PATH_SEP, PATH_SEP, PATH_SEP, cleanPath);
            file = fopen(stdPath, "rb");
        }
    }

    if (!file) {
        fprintf(stderr, "🐾 File '%s' not found\n", cleanPath);
        return;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* buf = (char*)malloc(size + 1);
    if (buf) {
        size_t read = fread(buf, 1, size, file);
        buf[read] = '\0';
        fclose(file);

        // Strip UTF-8 BOM (EF BB BF)
        if (read >= 3 &&
            (unsigned char)buf[0] == 0xEF &&
            (unsigned char)buf[1] == 0xBB &&
            (unsigned char)buf[2] == 0xBF) {
            memmove(buf, buf + 3, read - 2);
            buf[read - 3] = '\0';
        }

        Scanner previousScanner = scanner;
        initScanner(buf);
        while (peekToken().type != TOKEN_EOF) {
            parse_statement();
            if (lynx_error) {
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
                break;
            }
        }
        scanner = previousScanner;

        free(buf);
    } else {
        fclose(file);
    }
}

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    #endif
    
    // Initialize error state
    lynx_error_state.message = NULL;
    lynx_error_state.line = 0;
    lynx_error_state.col = 0;
    lynx_error = NULL;
    
    // Initialize try/catch state
    try_state.is_trying = 0;
    try_state.caught = 0;
    try_state.error_message = NULL;
    try_state.error_line = 0;
    try_state.error_col = 0;

    if (argc >= 2) {
        if (STRICMP(argv[1], "help") == 0 || STRICMP(argv[1], "--help") == 0) {
            show_help();
        } else if (STRICMP(argv[1], "--version") == 0) {
            printf("Lynx Engine %s\n", LYNX_VERSION);
        }
        #ifdef _WIN32
        else if (STRICMP(argv[1], "--update") == 0) {
            printf("🔄 Preparing update...\n");
            char tempInstaller[LYNX_MAX_PATH];
            sprintf(tempInstaller, "%s\\LynxInstaller.exe", getenv("TEMP"));
            const char* url = "https://github.com/justdev-chris/Lynx/releases/latest/download/LynxInstaller.exe";
            if (S_OK == URLDownloadToFileA(NULL, url, tempInstaller, 0, NULL)) {
                ShellExecuteA(NULL, "open", tempInstaller, NULL, NULL, SW_SHOWNORMAL);
                exit(0);
            } else {
                setErrorF("Update failed: Could not download installer");
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
        }
        #endif
        else if (STRICMP(argv[1], "init") == 0) {
            clearError();

            if (argc >= 3) {
                setVarString("__project_name", argv[2]);
            } else {
                setVarString("__project_name", "my_project");
            }
            if (argc >= 4) {
                setVarString("__author", argv[3]);
            } else {
                setVarString("__author", "Anonymous");
            }
            
            runFile("scripts/init.lnx", 0, NULL);
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "add") == 0) {
            if (argc >= 3) {
                setVarString("__pkg", argv[2]);
                runFile("scripts/add.lnx", 0, NULL);
            } else {
                setErrorF("Usage: lynx add <package>");
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "install") == 0) {
            runFile("scripts/install.lnx", 0, NULL);
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "remove") == 0) {
            if (argc >= 3) {
                setVarString("__pkg", argv[2]);
                runFile("scripts/remove.lnx", 0, NULL);
            } else {
                setErrorF("Usage: lynx remove <package>");
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "search") == 0) {
            if (argc >= 3) {
                setVarString("__term", argv[2]);
                runFile("scripts/search.lnx", 0, NULL);
            } else {
                setErrorF("Usage: lynx search <term>");
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "update") == 0) {
            runFile("scripts/update.lnx", 0, NULL);
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "publish") == 0) {
            runFile("scripts/publish.lnx", 0, NULL);
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "build") == 0) {
            runFile("src/main.lnx", 0, NULL);
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "fmt") == 0) {
            if (argc >= 3) {
                format_file(argv[2]);
            } else {
                setErrorF("Usage: lynx fmt <file.lnx>");
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
            unload_all_libs();
            cleanup_all();
            return 0;
        } else if (STRICMP(argv[1], "check") == 0) {
            if (argc >= 3) {
                check_file(argv[2]);
            } else {
                setErrorF("Usage: lynx check <file.lnx>");
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
            unload_all_libs();
            cleanup_all();
            return 0;
        } else {
            char scriptPath[256];
            snprintf(scriptPath, sizeof(scriptPath), "scripts/%s.lnx", argv[1]);
            FILE* f = fopen(scriptPath, "r");
            if (f) {
                fclose(f);
                runFile(scriptPath, 0, NULL);
            } else {
                FILE* test = fopen(argv[1], "r");
                if (test) {
                    fclose(test);
                    runFile(argv[1], 0, NULL);
                } else {
                    setErrorF("Unknown command: %s", argv[1]);
                    fprintf(stderr, "🐾 %s\n", lynx_error);
                    fprintf(stderr, "   Run 'lynx help' for available commands\n");
                    clearError();
                }
            }
            unload_all_libs();
            cleanup_all();
            return 0;
        }
    }

    char line[1024];
    printf("Lynx Engine %s | Type 'Help' for info\n", LYNX_VERSION);
    while (1) {
        printf("lynx > ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (STRICMP(line, "help") == 0) {
            show_help();
        } else if (STRICMP(line, "exit") == 0) {
            break;
        } else if (strstr(line, ".lnx") != NULL) {
            runFile(line, 0, NULL);
        } else {
            initScanner(line);
            parse_statement();
            if (lynx_error) {
                fprintf(stderr, "🐾 %s\n", lynx_error);
                clearError();
            }
        }
    }

    unload_all_libs();
    cleanup_all();
    printf("🐾 Goodbye!\n");
    return 0;
}
