#include "Stasis.h"

#define CONFIG_FILE L"Stasis.ini"
#define REG_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define REG_VAL L"Stasis"

void LoadSettings(void)
{
    g_State.cpuThreshold = GetPrivateProfileIntW(L"Settings", L"CpuThreshold", DEFAULT_CPU_THRESHOLD, CONFIG_FILE);
    g_State.memThreshold = GetPrivateProfileIntW(L"Settings", L"MemThreshold", DEFAULT_MEM_THRESHOLD, CONFIG_FILE);
    g_State.thawThreshold = GetPrivateProfileIntW(L"Settings", L"ThawThreshold", DEFAULT_THAW_THRESHOLD, CONFIG_FILE);
    g_State.autoMode = GetPrivateProfileIntW(L"Settings", L"AutoMode", 1, CONFIG_FILE);

    // 加载用户白名单
    WCHAR buf[2048] = {0};
    GetPrivateProfileStringW(L"Settings", L"Whitelist", L"", buf, 2048, CONFIG_FILE);
    if (wcslen(buf) > 0)
    {
        // 简单以分号分隔
        WCHAR* context = NULL;
        WCHAR* token = wcstok_s(buf, L";", &context);
        while (token)
        {
            while (*token == L' ') token++;
            if (*token)
            {
                EnterCriticalSection(&g_State.cs);
                WCHAR** newList = realloc(g_State.userWhitelist, (g_State.whitelistCount + 1) * sizeof(WCHAR*));
                if (!newList)
                {
                    LeaveCriticalSection(&g_State.cs);
                    break;
                }
                g_State.userWhitelist = newList;
                size_t len = wcslen(token) + 1;
                g_State.userWhitelist[g_State.whitelistCount] = malloc(len * sizeof(WCHAR));
                if (!g_State.userWhitelist[g_State.whitelistCount])
                {
                    LeaveCriticalSection(&g_State.cs);
                    break;
                }
                wcscpy_s(g_State.userWhitelist[g_State.whitelistCount], len, token);
                g_State.whitelistCount++;
                LeaveCriticalSection(&g_State.cs);
            }
            token = wcstok_s(NULL, L";", &context);
        }
    }
}

void SaveSettings(void)
{
    WCHAR buf[16];
    swprintf_s(buf, 16, L"%d", g_State.cpuThreshold);
    WritePrivateProfileStringW(L"Settings", L"CpuThreshold", buf, CONFIG_FILE);
    swprintf_s(buf, 16, L"%d", g_State.memThreshold);
    WritePrivateProfileStringW(L"Settings", L"MemThreshold", buf, CONFIG_FILE);
    swprintf_s(buf, 16, L"%d", g_State.thawThreshold);
    WritePrivateProfileStringW(L"Settings", L"ThawThreshold", buf, CONFIG_FILE);
    swprintf_s(buf, 16, L"%d", g_State.autoMode);
    WritePrivateProfileStringW(L"Settings", L"AutoMode", buf, CONFIG_FILE);

    // 保存白名单
    WCHAR whitelistStr[2048] = {0};
    EnterCriticalSection(&g_State.cs);
    for (int i = 0; i < g_State.whitelistCount; i++)
    {
        wcscat_s(whitelistStr, 2048, g_State.userWhitelist[i]);
        if (i < g_State.whitelistCount - 1)
            wcscat_s(whitelistStr, 2048, L";");
    }
    LeaveCriticalSection(&g_State.cs);
    WritePrivateProfileStringW(L"Settings", L"Whitelist", whitelistStr, CONFIG_FILE);
}

BOOL SetAutoStart(BOOL enable)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return FALSE;
    if (enable)
    {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        RegSetValueExW(hKey, REG_VAL, 0, REG_SZ, (BYTE*)path, (DWORD)(wcslen(path)+1)*sizeof(WCHAR));
    }
    else
        RegDeleteValueW(hKey, REG_VAL);
    RegCloseKey(hKey);
    return TRUE;
}

BOOL IsAutoStartEnabled(void)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return FALSE;
    DWORD type, size = 0;
    BOOL exists = (RegQueryValueExW(hKey, REG_VAL, NULL, &type, NULL, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

void ApplySettingsToUI(void)
{
    InvalidateRect(g_State.hMainWnd, NULL, TRUE);
}