// monitor_engine.c - 监控线程及调度算法
#include "Stasis.h"

static double GetTotalCpuUsage(void)
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

static double GetTotalMemUsage(void)
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
    double totalCpu = GetTotalCpuUsage();
    double totalMem = GetTotalMemUsage();
    if (totalCpu < g_State.cpuThreshold && totalMem < g_State.memThreshold) return;

    // 枚举进程CPU
    DWORD pids[1024];
    int count;
    EnumProcessesEx(pids, 1024, &count);
    ProcCpu* procs = malloc(count * sizeof(ProcCpu));
    int valid = 0;
    for (int i = 0; i < count; i++)
    {
        DWORD pid = pids[i];
        if (pid == 0 || pid == 4) continue;
        WCHAR name[MAX_PATH];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) continue;
        BOOL nameOk = FALSE;
        WCHAR path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, path, &size))
        {
            WCHAR* fname = wcsrchr(path, L'\\');
            wcscpy_s(name, MAX_PATH, fname ? fname + 1 : path);
            nameOk = TRUE;
        }
        CloseHandle(hProcess);
        if (!nameOk) continue;
        if (IsCriticalProcess(name) || IsProcessForeground(pid)) continue;
        if (IsProcessInUserWhitelist(name)) continue;

        // 简单CPU计算（此处简化，实际需维护历史）
        double cpu = 5.0; // 占位，实际可基于时间差计算
        procs[valid].pid = pid;
        procs[valid].cpu = cpu;
        valid++;
    }
    qsort(procs, valid, sizeof(ProcCpu), CompareProcCpu);

    // 依次冻结直到负载下降
    for (int i = 0; i < valid; i++)
    {
        double currentCpu = GetTotalCpuUsage();
        double currentMem = GetTotalMemUsage();
        if (currentCpu <= g_State.thawThreshold && currentMem <= g_State.thawThreshold)
            break;
        if (!SuspendProcessByPid(procs[i].pid)) continue;
        EnterCriticalSection(&g_State.cs);
        if (g_State.frozenCount < 1024)
            g_State.frozenStack[g_State.frozenCount++] = procs[i].pid;
        LeaveCriticalSection(&g_State.cs);
        Sleep(100); // 等待系统刷新
    }
    free(procs);
    // 刷新UI
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
    while (g_State.monitorRunning)
    {
        if (!g_State.pauseAuto && g_State.autoMode)
        {
            FreezeHighCpuProcesses();
            ThawProcessesIfNeeded();
        }
        // 前台检测：主动唤醒被切换到前台的冻结进程
        HWND hFg = GetForegroundWindow();
        DWORD fgPid;
        GetWindowThreadProcessId(hFg, &fgPid);
        EnterCriticalSection(&g_State.cs);
        for (int i = 0; i < g_State.frozenCount; i++)
        {
            if (g_State.frozenStack[i] == fgPid)
            {
                DWORD wakePid = fgPid;
                // 移除
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
        Sleep(MONITOR_INTERVAL);
    }
    return 0;
}