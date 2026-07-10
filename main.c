// main.c - 程序入口，启用DPI感知
#include "Stasis.h"

BOOL g_DebugMode = FALSE;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // DPI感知
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI *SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContext_t pSetProcessDpiAwarenessContext =
            (SetProcessDpiAwarenessContext_t)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pSetProcessDpiAwarenessContext)
            pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        else
            SetProcessDPIAware();
    } else
        SetProcessDPIAware();

    if (wcsstr(pCmdLine, L"--debug"))
        g_DebugMode = TRUE;

    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"Stasis_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED) {
        if (hMutex) CloseHandle(hMutex);
        HWND hPrevWnd = FindWindowW(L"StasisMainWindowClass", L"Stasis");
        if (hPrevWnd) { SetForegroundWindow(hPrevWnd); ShowWindow(hPrevWnd, SW_RESTORE); }
        return 0;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    memset(&g_State, 0, sizeof(g_State));
    InitializeCriticalSection(&g_State.cs);
    g_State.autoMode = FALSE;
    g_State.cpuThreshold = DEFAULT_CPU_THRESHOLD;
    g_State.memThreshold = DEFAULT_MEM_THRESHOLD;
    g_State.thawThreshold = DEFAULT_THAW_THRESHOLD;
    g_State.monitorRunning = FALSE;
    g_State.trayVisible = FALSE;
    g_State.pauseAuto = FALSE;

    EnableDebugPrivilege();
    if (!InitProcessAPI()) {
        MessageBoxW(NULL, L"无法加载进程挂起/恢复API，程序无法运行。", L"错误", MB_ICONERROR);
        return 1;
    }

    InitLog();
    LoadSettings();

    HWND hMain = CreateMainWindow(hInstance);
    if (!hMain) return 1;
    g_State.hMainWnd = hMain;

    g_State.monitorRunning = TRUE;
    g_State.hMonitorThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
    SetTimer(hMain, WM_WATCHDOG_TIMER, 1000, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_State.monitorRunning = FALSE;
    if (g_State.hMonitorThread) { WaitForSingleObject(g_State.hMonitorThread, 2000); CloseHandle(g_State.hMonitorThread); }
    ForceThawAll();
    RemoveTrayIcon();
    CloseLog();
    DeleteCriticalSection(&g_State.cs);
    CloseHandle(hMutex);
    return 0;
}