// process_manager.c - 进程操作核心
#include "Stasis.h"

pNtSuspendProcess NtSuspendProcess = NULL;
pNtResumeProcess NtResumeProcess = NULL;

const WCHAR* SystemWhitelist[] = {
    L"System", L"Idle", L"csrss.exe", L"winlogon.exe",
    L"services.exe", L"lsass.exe", L"svchost.exe",
    L"explorer.exe", L"dwm.exe", L"taskmgr.exe"
};

BOOL InitProcessAPI(void)
{
    HMODULE hNtdll = LoadLibraryW(L"ntdll.dll");
    if (!hNtdll) return FALSE;
    NtSuspendProcess = (pNtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
    NtResumeProcess = (pNtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
    return (NtSuspendProcess && NtResumeProcess);
}

BOOL EnableDebugPrivilege(void)
{
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        return FALSE;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ret = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    CloseHandle(hToken);
    return ret;
}

BOOL SuspendProcessByPid(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!hProcess) return FALSE;
    // 简单同步调用，无超时保护（可扩展）
    BOOL ret = NtSuspendProcess(hProcess);
    CloseHandle(hProcess);
    if (ret) LogEvent(L"Frozen PID %lu", pid);
    return ret;
}

BOOL ResumeProcessByPid(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!hProcess) return FALSE;
    BOOL ret = NtResumeProcess(hProcess);
    CloseHandle(hProcess);
    if (ret) LogEvent(L"Resumed PID %lu", pid);
    return ret;
}

BOOL EnumProcessesEx(DWORD* pids, int maxCount, int* outCount)
{
    DWORD cbNeeded;
    if (!EnumProcesses(pids, maxCount * sizeof(DWORD), &cbNeeded))
        return FALSE;
    *outCount = cbNeeded / sizeof(DWORD);
    return TRUE;
}

BOOL GetProcessMemoryKB(DWORD pid, SIZE_T* memKB)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return FALSE;
    PROCESS_MEMORY_COUNTERS_EX pmc;
    BOOL ret = GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    if (ret) *memKB = pmc.WorkingSetSize / 1024;
    CloseHandle(hProcess);
    return ret;
}

BOOL IsCriticalProcess(const WCHAR* name)
{
    for (int i = 0; i < SYSTEM_WHITELIST_COUNT; i++)
        if (_wcsicmp(name, SystemWhitelist[i]) == 0) return TRUE;
    return FALSE; // 注意：用户白名单在逻辑上应单独判断，此处仅检查系统关键进程
}

BOOL IsProcessInUserWhitelist(const WCHAR* name)
{
    EnterCriticalSection(&g_State.cs);
    for (int i = 0; i < g_State.whitelistCount; i++)
        if (_wcsicmp(name, g_State.userWhitelist[i]) == 0)
        {
            LeaveCriticalSection(&g_State.cs);
            return TRUE;
        }
    LeaveCriticalSection(&g_State.cs);
    return FALSE;
}

BOOL IsProcessForeground(DWORD pid)
{
    HWND hFg = GetForegroundWindow();
    DWORD fgPid;
    GetWindowThreadProcessId(hFg, &fgPid);
    return (fgPid == pid);
}

BOOL IsCurrentProcess(DWORD pid)
{
    return pid == GetCurrentProcessId();
}