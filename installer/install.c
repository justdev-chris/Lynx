#include <stdio.h>
#include <windows.h>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")

void download(const char* url, const char* dest) {
    if (S_OK == URLDownloadToFileA(NULL, url, dest, 0, NULL)) {
        printf("[+] Success: %s\n", dest);
    } else {
        printf("[!] Failed: %s\n", url);
    }
}

int main() {
    printf("🐾 Lynx Language: Global Installer\n");
    printf("----------------------------------\n");

    char base[MAX_PATH], std[MAX_PATH];
    sprintf(base, "%s\\LynxLang", getenv("APPDATA"));
    sprintf(std, "%s\\std", base);

    // Create system directories in AppData
    CreateDirectory(base, NULL);
    CreateDirectory(std, NULL);

    char exe_d[MAX_PATH], math_d[MAX_PATH], color_d[MAX_PATH];
    sprintf(exe_d, "%s\\lynx.exe", base);
    sprintf(math_d, "%s\\math.lnx", std);
    sprintf(color_d, "%s\\colors.lnx", std);

    // The links to your raw files and releases
    const char* u_exe = "https://github.com/justdev-chris/Lynx/releases/download/v1.3/lynx.exe";
    const char* u_math = "https://raw.githubusercontent.com/justdev-chris/Lynx/main/std/math.lnx";
    const char* u_color = "https://raw.githubusercontent.com/justdev-chris/Lynx/main/std/colors.lnx";

    printf("📦 Downloading core components to AppData...\n");
    download(u_exe, exe_d);
    download(u_math, math_d);
    download(u_color, color_d);

    // Registry: Add LynxLang to User PATH
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Environment", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
        char path[8192] = {0};
        DWORD size = sizeof(path);
        RegQueryValueEx(hKey, "Path", NULL, NULL, (LPBYTE)path, &size);

        if (strstr(path, base) == NULL) {
            strcat(path, ";");
            strcat(path, base);
            RegSetValueEx(hKey, "Path", 0, REG_EXPAND_SZ, (LPBYTE)path, strlen(path) + 1);
            
            // Broadcast change so current windows might see it
            SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
            printf("[+] Added Lynx to PATH.\n");
        }
        RegCloseKey(hKey);
    }

    printf("\n✅ Installation/Update Complete!\n");
    printf("Restart your CMD and type 'lynx --version'\n");
    
    printf("\nPress Enter to exit...");
    getchar(); 

    return 0;
}
