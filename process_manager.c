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

DWORD WINAPI SuspendThreadWrapper(LPVOID param)
{
    HANDLE hProcess = (HANDLE)param;
    return NtSuspendProcess(hProcess);
}

DWORD WINAPI ResumeThreadWrapper(LPVOID param)
{
    HANDLE hProcess = (HANDLE)param;
    return NtResumeProcess(hProcess);
}

BOOL SuspendProcessByPid(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME | SYNCHRONIZE, FALSE, pid);
    if (!hProcess) return FALSE;
    HANDLE hThread = CreateThread(NULL, 0, SuspendThreadWrapper, hProcess, 0, NULL);
    if (!hThread) {
        CloseHandle(hProcess);
        return FALSE;
    }
    DWORD wait = WaitForSingleObject(hThread, SUSPEND_TIMEOUT_MS);
    DWORD ret = FALSE;
    if (wait == WAIT_OBJECT_0) {
        GetExitCodeThread(hThread, &ret);
    } else {
        TerminateThread(hThread, 0);
        ret = FALSE;
    }
    CloseHandle(hThread);
    CloseHandle(hProcess);
    if (ret) LogEvent(L"Frozen PID %lu", pid);
    return ret;
}

BOOL ResumeProcessByPid(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME | SYNCHRONIZE, FALSE, pid);
    if (!hProcess) return FALSE;
    HANDLE hThread = CreateThread(NULL, 0, ResumeThreadWrapper, hProcess, 0, NULL);
    if (!hThread) {
        CloseHandle(hProcess);
        return FALSE;
    }
    DWORD wait = WaitForSingleObject(hThread, SUSPEND_TIMEOUT_MS);
    DWORD ret = FALSE;
    if (wait == WAIT_OBJECT_0) {
        GetExitCodeThread(hThread, &ret);
    } else {
        TerminateThread(hThread, 0);
        ret = FALSE;
    }
    CloseHandle(hThread);
    CloseHandle(hProcess);
    if (ret) LogEvent(L"Resumed PID %lu", pid);
    return ret;
}

BOOL EnumProcessesEx(DWORD* pids, int maxCount, int* outCount)
{
    DWORD cbNeeded;
    if (!EnumProcesses(pids, maxCount * sizeof(DWORD), &cbNeeded))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            *outCount = maxCount;
            g_State.enumTruncated = TRUE;
            return TRUE;
        }
        return FALSE;
    }
    *outCount = cbNeeded / sizeof(DWORD);
    g_State.enumTruncated = FALSE;
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
    return FALSE;
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
    if (!hFg) return FALSE;
    DWORD fgPid;
    GetWindowThreadProcessId(hFg, &fgPid);
    return (fgPid == pid);
}

BOOL IsCurrentProcess(DWORD pid)
{
    return pid == GetCurrentProcessId();
}