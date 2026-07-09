// Stasis.h - 全局头文件，包含公共类型、宏与外部声明
#ifndef STASIS_H
#define STASIS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 窗口与资源ID
#define IDC_LISTVIEW        1001
#define IDC_TOGGLE_AUTO     1002
#define IDC_SLIDER_CPU      1003
#define IDC_SLIDER_MEM      1004
#define IDC_SLIDER_THAW     1005
#define IDC_BTN_WHITELIST   1006
#define IDC_BTN_TRAY        1007
#define IDC_BTN_OPTIONS     1008
#define IDC_TITLEBAR_CLOSE  1009
#define IDC_TITLEBAR_MIN    1010
#define IDC_SYSLINK_FEEDBACK 1011
#define IDC_TITLEBAR_TITLE  1012
#define WM_TRAYICON         (WM_APP + 1)
#define WM_WATCHDOG_TIMER   (WM_APP + 2)
#define ID_TRAY_SHOW        2001
#define ID_TRAY_PAUSE       2002
#define ID_TRAY_EXIT        2003

// 阈值默认值
#define DEFAULT_CPU_THRESHOLD       85
#define DEFAULT_MEM_THRESHOLD       90
#define DEFAULT_THAW_THRESHOLD      60

// 白名单系统进程
#define SYSTEM_WHITELIST_COUNT 7
extern const WCHAR* SystemWhitelist[SYSTEM_WHITELIST_COUNT];

// 日志行数
#define LOG_MAX_LINES 500
// 监控间隔 ms
#define MONITOR_INTERVAL 500
// 看门狗超时（ms）
#define WATCHDOG_TIMEOUT 5000
// NtSuspendProcess 超时（简化处理，不使用）
#define SUSPEND_TIMEOUT_MS 3000

// 进程状态图标字符
#define ICON_FROZEN L"❄️"
#define ICON_RUNNING L"▶️"

// 全局状态结构体
typedef struct {
    // 监控配置
    BOOL autoMode;
    int cpuThreshold;
    int memThreshold;
    int thawThreshold;
    // 白名单 (动态分配)
    WCHAR** userWhitelist;
    int whitelistCount;
    // 冻结栈 (LIFO)，存 PID
    DWORD frozenStack[1024];
    int frozenCount;
    // 同步
    CRITICAL_SECTION cs;
    // 监控线程句柄
    HANDLE hMonitorThread;
    volatile BOOL monitorRunning;
    // 主窗口句柄
    HWND hMainWnd;
    HWND hListView;
    // 托盘
    BOOL trayVisible;
    BOOL pauseAuto; // 托盘应急暂停自动模式
    // 看门狗应答计数
    volatile LONG watchdogCounter;
    // 日志文件句柄
    FILE* logFile;
    // 亚克力注册键
    BOOL acrylicEnabled;
} AppState;

// 全局状态 extern 声明（定义在 app_window.c）
extern AppState g_State;
extern BOOL g_DebugMode;
void DebugLog(const WCHAR* format, ...);

// 模块函数声明

// app_window.c
HWND CreateMainWindow(HINSTANCE hInstance);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateUIFromState(void);
void RefreshListView(void);

// process_manager.c
typedef BOOL (WINAPI* pNtSuspendProcess)(HANDLE);
typedef BOOL (WINAPI* pNtResumeProcess)(HANDLE);
extern pNtSuspendProcess NtSuspendProcess;
extern pNtResumeProcess NtResumeProcess;
BOOL InitProcessAPI(void);
BOOL SuspendProcessByPid(DWORD pid);
BOOL ResumeProcessByPid(DWORD pid);
BOOL EnumProcessesEx(DWORD* pids, int maxCount, int* outCount);
BOOL GetProcessMemoryKB(DWORD pid, SIZE_T* memKB);
BOOL IsCriticalProcess(const WCHAR* name);
BOOL IsProcessInUserWhitelist(const WCHAR* name);
BOOL IsProcessForeground(DWORD pid);
BOOL EnableDebugPrivilege(void);

// monitor_engine.c
DWORD WINAPI MonitorThread(LPVOID param);
void FreezeHighCpuProcesses(void);
void ThawProcessesIfNeeded(void);
void ForceThawAll(void);
double GetTotalCpuUsage(void);
double GetTotalMemUsage(void);

// ui_controls.c
void DrawGradientProgressBar(HDC hdc, RECT rc, double value, COLORREF color1, COLORREF color2);
void DrawRoundedButton(HDC hdc, RECT rc, const WCHAR* text, BOOL hover, BOOL pressed);
void DrawToggleSwitch(HDC hdc, RECT rc, BOOL state);
void DrawSlider(HDC hdc, RECT rc, int min, int max, int value);
void InitCustomControls(void);
void PaintCustomUI(HWND hwnd, HDC hdc);
LRESULT CALLBACK SliderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// tray_icon.c
void CreateTrayIcon(HWND hwnd);
void UpdateTrayIcon(int frozenCount);
void RemoveTrayIcon(void);
void ShowTrayMenu(HWND hwnd);

// settings_store.c
void LoadSettings(void);
void SaveSettings(void);
BOOL SetAutoStart(BOOL enable);
BOOL IsAutoStartEnabled(void);
void ApplySettingsToUI(void);

// log.c
void LogEvent(const WCHAR* format, ...);
void InitLog(void);
void CloseLog(void);

#endif