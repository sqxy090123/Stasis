// monitor_engine.c - 监控线程及调度算法
#include "Stasis.h"

double GetTotalCpuUsage(void)
{
    static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;
    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);
    ULONGLONG idleTime = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
    ULONGLONG kernelTime = ((ULONGLONG)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
    ULONGLONG userTime = ((ULONGLONG)user.dwHighDateTime << 32) | user.dwLowDateTime;
    if (prevIdle == 0) { prevIdle = idleTime; prevKernel = kernelTime; prevUser = userTime; return 0.0; }
    ULONGLONG idleDiff = idleTime - prevIdle;
    ULONGLONG kernelDiff = kernelTime - prevKernel;
    ULONGLONG userDiff = userTime - prevUser;
    ULONGLONG totalDiff = kernelDiff + userDiff;
    prevIdle = idleTime; prevKernel = kernelTime; prevUser = userTime;
    if (totalDiff == 0) return 0.0;
    return (double)(totalDiff - idleDiff) / totalDiff * 100.0;
}

double GetTotalMemUsage(void)
{
    MEMORYSTATUSEX mem = { sizeof(MEMORYSTATUSEX) };
    GlobalMemoryStatusEx(&mem);
    return (double)mem.dwMemoryLoad;
}

typedef struct {
    DWORD pid;
    double cpu;
} ProcCpu;

static int CompareProcCpu(const void* a, const void* b)
{
    double diff = ((ProcCpu*)b)->cpu - ((ProcCpu*)a)->cpu;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

void FreezeHighCpuProcesses(void)
{
    DebugLog(L"FreezeHighCpuProcesses called");
    double totalCpu = GetTotalCpuUsage();
    double totalMem = GetTotalMemUsage();
    if (totalCpu < g_State.cpuThreshold && totalMem < g_State.memThreshold) return;

    DWORD pids[1024];
    int count;
    EnumProcessesEx(pids, 1024, &count);
    ProcCpu* procs = malloc(count * sizeof(ProcCpu));
    if (!procs) return;
    int valid = 0;
    for (int i = 0; i < count; i++)
    {
        DWORD pid = pids[i];
        if (pid == 0 || pid == 4) continue;
        WCHAR name[MAX_PATH];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) continue;
        WCHAR path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, path, &size))
        {
            WCHAR* fname = wcsrchr(path, L'\\');
            wcscpy_s(name, MAX_PATH, fname ? fname + 1 : path);
        }
        else {
            CloseHandle(hProcess);
            continue;
        }
        CloseHandle(hProcess);

        if (IsCriticalProcess(name) || IsProcessForeground(pid)) continue;
        if (IsProcessInUserWhitelist(name)) continue;
        if (IsCurrentProcess(pid)) continue;

        procs[valid].pid = pid;
        procs[valid].cpu = 5.0; // 简化占位
        valid++;
    }
    qsort(procs, valid, sizeof(ProcCpu), CompareProcCpu);

    int maxFreezeThisRound = 5;
    int frozenThisRound = 0;

    for (int i = 0; i < valid && frozenThisRound < maxFreezeThisRound; i++)
    {
        double curCpu = GetTotalCpuUsage();
        double curMem = GetTotalMemUsage();
        if (curCpu <= g_State.thawThreshold && curMem <= g_State.thawThreshold)
            break;
        if (!SuspendProcessByPid(procs[i].pid)) continue;
        EnterCriticalSection(&g_State.cs);
        if (g_State.frozenCount < 1024)
            g_State.frozenStack[g_State.frozenCount++] = procs[i].pid;
        LeaveCriticalSection(&g_State.cs);
        frozenThisRound++;
        Sleep(100);
    }
    free(procs);
    PostMessage(g_State.hMainWnd, WM_USER + 1, 0, 0);
}

void ThawProcessesIfNeeded(void)
{
    double totalCpu = GetTotalCpuUsage();
    double totalMem = GetTotalMemUsage();
    if (totalCpu > g_State.thawThreshold || totalMem > g_State.thawThreshold) return;

    EnterCriticalSection(&g_State.cs);
    while (g_State.frozenCount > 0)
    {
        DWORD pid = g_State.frozenStack[--g_State.frozenCount];
        LeaveCriticalSection(&g_State.cs);
        ResumeProcessByPid(pid);
        EnterCriticalSection(&g_State.cs);
        if (GetTotalCpuUsage() > g_State.thawThreshold || GetTotalMemUsage() > g_State.thawThreshold)
            break;
    }
    LeaveCriticalSection(&g_State.cs);
}

void ForceThawAll(void)
{
    EnterCriticalSection(&g_State.cs);
    while (g_State.frozenCount > 0)
    {
        DWORD pid = g_State.frozenStack[--g_State.frozenCount];
        LeaveCriticalSection(&g_State.cs);
        ResumeProcessByPid(pid);
        EnterCriticalSection(&g_State.cs);
    }
    LeaveCriticalSection(&g_State.cs);
}

DWORD WINAPI MonitorThread(LPVOID param)
{
    LONG lastWatchdogCounter = 0;
    DWORD lastCheck = 0;
    while (g_State.monitorRunning)
    {
        DWORD now = GetTickCount();
        if (now - lastCheck > 2000)
        {
            lastCheck = now;
            LONG current = InterlockedExchangeAdd(&g_State.watchdogCounter, 0);
            if (current == lastWatchdogCounter && lastWatchdogCounter != 0)
            {
                ForceThawAll();
                PostMessage(g_State.hMainWnd, WM_WATCHDOG_ALERT, 0, 0);
            }
            lastWatchdogCounter = current;
        }

        if (!g_State.pauseAuto && g_State.autoMode)
        {
            FreezeHighCpuProcesses();
            ThawProcessesIfNeeded();
        }

        BOOL hasFrozen;
        EnterCriticalSection(&g_State.cs);
        hasFrozen = (g_State.frozenCount > 0);
        LeaveCriticalSection(&g_State.cs);

        if (hasFrozen)
        {
            HWND hFg = GetForegroundWindow();
            if (hFg)
            {
                DWORD fgPid;
                GetWindowThreadProcessId(hFg, &fgPid);
                EnterCriticalSection(&g_State.cs);
                for (int i = 0; i < g_State.frozenCount; i++)
                {
                    if (g_State.frozenStack[i] == fgPid)
                    {
                        DWORD wakePid = fgPid;
                        memmove(&g_State.frozenStack[i], &g_State.frozenStack[i+1],
                            (g_State.frozenCount - i - 1) * sizeof(DWORD));
                        g_State.frozenCount--;
                        LeaveCriticalSection(&g_State.cs);
                        ResumeProcessByPid(wakePid);
                        EnterCriticalSection(&g_State.cs);
                        break;
                    }
                }
                LeaveCriticalSection(&g_State.cs);
            }
        }

        Sleep(MONITOR_INTERVAL);
    }
    return 0;
}