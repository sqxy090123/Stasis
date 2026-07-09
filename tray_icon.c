#include "Stasis.h"

static NOTIFYICONDATAW nid = {0};

void CreateTrayIcon(HWND hwnd)
{
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(GetModuleHandleW(NULL), L"APP_ICON");
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"Stasis");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void UpdateTrayIcon(int frozenCount)
{
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void RemoveTrayIcon(void)
{
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"显示主界面");
    AppendMenuW(hMenu, MF_STRING | (g_State.pauseAuto ? MF_CHECKED : 0), ID_TRAY_PAUSE, L"暂停自动模式");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"全部解冻并退出");
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    switch (cmd)
    {
    case ID_TRAY_SHOW:
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        g_State.trayVisible = FALSE;
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        break;
    case ID_TRAY_PAUSE:
        g_State.pauseAuto = !g_State.pauseAuto;
        break;
    case ID_TRAY_EXIT:
        DestroyWindow(hwnd);
        break;
    }
    DestroyMenu(hMenu);
}