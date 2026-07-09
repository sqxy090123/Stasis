// main.c - 程序入口
#include "Stasis.h"

BOOL g_DebugMode = FALSE;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    if (wcsstr(pCmdLine, L"--debug"))
        g_DebugMode = TRUE;

    // 单实例检查
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"Stasis_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED)
    {
        if (hMutex) CloseHandle(hMutex);
        HWND hPrevWnd = FindWindowW(L"StasisMainWindowClass", L"Stasis");
        if (hPrevWnd)
        {
            SetForegroundWindow(hPrevWnd);
            ShowWindow(hPrevWnd, SW_RESTORE);
        }
        return 0;
    }

    // 初始化公共控件（ListView等）
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // 初始化全局状态
    memset(&g_State, 0, sizeof(g_State));
    InitializeCriticalSection(&g_State.cs);
    g_State.autoMode = TRUE;
    g_State.cpuThreshold = DEFAULT_CPU_THRESHOLD;
    g_State.memThreshold = DEFAULT_MEM_THRESHOLD;
    g_State.thawThreshold = DEFAULT_THAW_THRESHOLD;
    g_State.monitorRunning = FALSE;
    g_State.trayVisible = FALSE;
    g_State.pauseAuto = FALSE;

    // 提升权限
    EnableDebugPrivilege();

    // 加载动态API
    if (!InitProcessAPI())
    {
        MessageBoxW(NULL, L"无法加载进程挂起/恢复API，程序无法运行。", L"错误", MB_ICONERROR);
        return 1;
    }

    // 初始化日志
    InitLog();
    // 加载设置
    LoadSettings();

    // 创建主窗口
    HWND hMain = CreateMainWindow(hInstance);
    if (!hMain) return 1;
    g_State.hMainWnd = hMain;

    // 启动监控线程
    g_State.monitorRunning = TRUE;
    g_State.hMonitorThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);

    // 设置看门狗定时器
    SetTimer(hMain, WM_WATCHDOG_TIMER, 1000, NULL);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 清理
    g_State.monitorRunning = FALSE;
    if (g_State.hMonitorThread)
    {
        WaitForSingleObject(g_State.hMonitorThread, 2000);
        CloseHandle(g_State.hMonitorThread);
    }
    // 强制恢复所有冻结进程后再退出
    ForceThawAll();
    RemoveTrayIcon();
    CloseLog();
    DeleteCriticalSection(&g_State.cs);
    CloseHandle(hMutex);
    return 0;
}