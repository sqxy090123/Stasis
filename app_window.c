// app_window.c - 主窗口创建、窗口过程及UI更新
#include "Stasis.h"

// 全局状态定义
AppState g_State = {0};

// 亚克力效果相关结构（Win10+）
typedef struct _ACCENTPOLICY {
    DWORD AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENTPOLICY;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

#define WCA_ACCENT_POLICY 19
#define ACCENT_ENABLE_BLURBEHIND 3

typedef BOOL (WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
static pSetWindowCompositionAttribute SetWindowCompositionAttribute = NULL;

static void EnableAcrylic(HWND hwnd)
{
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser)
    {
        SetWindowCompositionAttribute = (pSetWindowCompositionAttribute)
            GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (SetWindowCompositionAttribute)
        {
            ACCENTPOLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, 0, 0 };
            WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };
            SetWindowCompositionAttribute(hwnd, &data);
            g_State.acrylicEnabled = TRUE;
        }
    }
}

// 白名单对话框（简单实现，使用MessageBox替代，实际可扩展为完整对话框）
static void OpenWhitelistDialog(HWND hParent)
{
    MessageBoxW(hParent, L"白名单管理功能待完善，当前可通过编辑 Stasis.ini 添加白名单。", L"白名单", MB_OK);
}

// 主窗口过程
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, L"APP_ICON"); // resource.rc中定义
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"StasisMainWindowClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_LAYERED,
        L"StasisMainWindowClass", L"Stasis",
        WS_POPUP | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 500,
        NULL, NULL, hInstance, NULL);

    if (hwnd)
    {
        EnableAcrylic(hwnd);
        SetWindowPos(hwnd, NULL, 0, 0, 700, 500, SWP_NOMOVE | SWP_NOZORDER);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

// 窗口过程
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // 创建ListView
        HWND hList = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED,
            10, 90, 680, 280, hwnd, (HMENU)IDC_LISTVIEW, NULL, NULL);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
        g_State.hListView = hList;

        // 插入列
        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.cx = 60; lvc.pszText = L"状态"; ListView_InsertColumn(hList, 0, &lvc);
        lvc.cx = 150; lvc.pszText = L"进程名"; ListView_InsertColumn(hList, 1, &lvc);
        lvc.cx = 80; lvc.pszText = L"PID"; ListView_InsertColumn(hList, 2, &lvc);
        lvc.cx = 80; lvc.pszText = L"CPU%"; ListView_InsertColumn(hList, 3, &lvc);
        lvc.cx = 100; lvc.pszText = L"内存(KB)"; ListView_InsertColumn(hList, 4, &lvc);

        // 创建自绘按钮/滑块控件（全部使用BS_OWNERDRAW）
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            10, 390, 80, 30, hwnd, (HMENU)IDC_TOGGLE_AUTO, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            100, 390, 100, 30, hwnd, (HMENU)IDC_SLIDER_CPU, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            210, 390, 100, 30, hwnd, (HMENU)IDC_SLIDER_MEM, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            320, 390, 100, 30, hwnd, (HMENU)IDC_SLIDER_THAW, NULL, NULL);
        CreateWindowW(L"BUTTON", L"白名单", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            450, 390, 70, 30, hwnd, (HMENU)IDC_BTN_WHITELIST, NULL, NULL);
        CreateWindowW(L"BUTTON", L"托盘", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            530, 390, 60, 30, hwnd, (HMENU)IDC_BTN_TRAY, NULL, NULL);
        CreateWindowW(L"BUTTON", L"选项", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            600, 390, 60, 30, hwnd, (HMENU)IDC_BTN_OPTIONS, NULL, NULL);

        // 反馈超链接
        CreateWindowW(L"SysLink", L"<a href=\"https://github.com/sqxy090123/Stasis/issues\">遇到问题？提交反馈</a>",
            WS_CHILD | WS_VISIBLE, 10, 430, 200, 20, hwnd, (HMENU)IDC_SYSLINK_FEEDBACK, NULL, NULL);

        // 子类化滑块控件
        HWND hSliderCpu = GetDlgItem(hwnd, IDC_SLIDER_CPU);
        SetWindowLongPtr(hSliderCpu, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtr(hSliderCpu, GWLP_WNDPROC));
        SetWindowLongPtr(hSliderCpu, GWLP_WNDPROC, (LONG_PTR)SliderSubclassProc);
        HWND hSliderMem = GetDlgItem(hwnd, IDC_SLIDER_MEM);
        SetWindowLongPtr(hSliderMem, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtr(hSliderMem, GWLP_WNDPROC));
        SetWindowLongPtr(hSliderMem, GWLP_WNDPROC, (LONG_PTR)SliderSubclassProc);
        HWND hSliderThaw = GetDlgItem(hwnd, IDC_SLIDER_THAW);
        SetWindowLongPtr(hSliderThaw, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtr(hSliderThaw, GWLP_WNDPROC));
        SetWindowLongPtr(hSliderThaw, GWLP_WNDPROC, (LONG_PTR)SliderSubclassProc);

        // 初始化托盘
        CreateTrayIcon(hwnd);
        ApplySettingsToUI();
        UpdateUIFromState();
        break;
    }
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_BUTTON)
        {
            int id = (int)lpdis->CtlID;
            RECT rc = lpdis->rcItem;
            HDC hdc = lpdis->hDC;
            WCHAR text[32];
            GetWindowTextW(lpdis->hwndItem, text, 32);
            BOOL hover = (lpdis->itemState & ODS_HOTLIGHT) != 0;
            BOOL pressed = (lpdis->itemState & ODS_SELECTED) != 0;

            switch (id)
            {
            case IDC_TOGGLE_AUTO:
                DrawToggleSwitch(hdc, rc, g_State.autoMode);
                break;
            case IDC_SLIDER_CPU:
                DrawSlider(hdc, rc, 50, 100, g_State.cpuThreshold);
                break;
            case IDC_SLIDER_MEM:
                DrawSlider(hdc, rc, 50, 100, g_State.memThreshold);
                break;
            case IDC_SLIDER_THAW:
                DrawSlider(hdc, rc, 30, 80, g_State.thawThreshold);
                break;
            case IDC_BTN_WHITELIST:
                DrawRoundedButton(hdc, rc, L"白名单", hover, pressed);
                break;
            case IDC_BTN_TRAY:
                DrawRoundedButton(hdc, rc, L"托盘", hover, pressed);
                break;
            case IDC_BTN_OPTIONS:
                DrawRoundedButton(hdc, rc, L"选项", hover, pressed);
                break;
            }
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        switch (id)
        {
        case IDC_BTN_WHITELIST:
            OpenWhitelistDialog(hwnd);
            break;
        case IDC_BTN_TRAY:
            ShowWindow(hwnd, SW_HIDE);
            g_State.trayVisible = TRUE;
            SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
            UpdateTrayIcon(g_State.frozenCount);
            break;
        case IDC_BTN_OPTIONS:
        {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 3001, L"开机自启动");
            if (IsAutoStartEnabled()) CheckMenuItem(hMenu, 3001, MF_CHECKED);
            POINT pt; GetCursorPos(&pt);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            break;
        }
        case 3001:
            SetAutoStart(!IsAutoStartEnabled());
            break;
        case IDC_SYSLINK_FEEDBACK:
            ShellExecuteW(NULL, L"open", L"https://github.com/sqxy090123/Stasis/issues", NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        break;
    }
    case WM_NOTIFY:
    {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->idFrom == IDC_LISTVIEW && pnm->code == NM_CLICK)
        {
            LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
            if (lpnmitem->iItem != -1 && lpnmitem->iSubItem == 0)
            {
                WCHAR buf[20];
                ListView_GetItemText(g_State.hListView, lpnmitem->iItem, 2, buf, 20);
                DWORD pid = _wtoi(buf);
                WCHAR name[MAX_PATH];
                ListView_GetItemText(g_State.hListView, lpnmitem->iItem, 1, name, MAX_PATH);
                if (IsCriticalProcess(name))
                {
                    if (IDYES != MessageBoxW(hwnd, L"确定要手动操作系统关键进程吗？", L"警告", MB_YESNO | MB_ICONWARNING))
                        break;
                }
                BOOL isFrozen = FALSE;
                EnterCriticalSection(&g_State.cs);
                for (int i = 0; i < g_State.frozenCount; i++)
                    if (g_State.frozenStack[i] == pid) { isFrozen = TRUE; break; }
                LeaveCriticalSection(&g_State.cs);
                if (isFrozen)
                    ResumeProcessByPid(pid);
                else
                    SuspendProcessByPid(pid);
                RefreshListView();
            }
        }
        break;
    }
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu(hwnd);
        else if (lParam == WM_LBUTTONDBLCLK)
        {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            g_State.trayVisible = FALSE;
            SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        }
        break;
    case WM_WATCHDOG_TIMER:
        InterlockedIncrement(&g_State.watchdogCounter);
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        g_State.trayVisible = TRUE;
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
        UpdateTrayIcon(g_State.frozenCount);
        {
            NOTIFYICONDATAW nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_INFO;
            wcscpy_s(nid.szInfo, _countof(nid.szInfo), L"Stasis 已驻留后台，继续保护系统。");
            nid.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &nid);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, WM_WATCHDOG_TIMER);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void UpdateUIFromState(void)
{
    InvalidateRect(g_State.hMainWnd, NULL, TRUE);
}

void RefreshListView(void)
{
    ListView_DeleteAllItems(g_State.hListView);
    DWORD pids[1024];
    int count = 0;
    EnumProcessesEx(pids, 1024, &count);

    for (int i = 0; i < count; i++)
    {
        DWORD pid = pids[i];
        WCHAR name[MAX_PATH] = L"<unknown>";
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess)
        {
            WCHAR path[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, path, &size))
            {
                WCHAR* fname = wcsrchr(path, L'\\');
                if (fname) wcscpy_s(name, MAX_PATH, fname + 1);
                else wcscpy_s(name, MAX_PATH, path);
            }
            CloseHandle(hProcess);
        }

        BOOL frozen = FALSE;
        EnterCriticalSection(&g_State.cs);
        for (int j = 0; j < g_State.frozenCount; j++)
            if (g_State.frozenStack[j] == pid) { frozen = TRUE; break; }
        LeaveCriticalSection(&g_State.cs);

        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = frozen ? ICON_FROZEN : ICON_RUNNING;
        ListView_InsertItem(g_State.hListView, &lvi);

        ListView_SetItemText(g_State.hListView, i, 1, name);

        WCHAR buf[32];
        swprintf_s(buf, 32, L"%lu", pid);
        ListView_SetItemText(g_State.hListView, i, 2, buf);

        ListView_SetItemText(g_State.hListView, i, 3, L"0.0");

        SIZE_T memKB = 0;
        GetProcessMemoryKB(pid, &memKB);
        swprintf_s(buf, 32, L"%llu", (unsigned long long)memKB);
        ListView_SetItemText(g_State.hListView, i, 4, buf);
    }
}