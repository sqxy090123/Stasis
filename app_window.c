// app_window.c - 主窗口创建、窗口过程及UI更新（现代化改进）
#include "Stasis.h"

// 全局状态定义
AppState g_State = {0};

// 亚克力效果结构（同前）
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

// 自定义绘制参数
#define TITLE_BAR_HEIGHT 35
#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 500

// 全局字体
static HFONT g_hTitleFont = NULL;

// 窗口过程声明
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, L"APP_ICON");
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"StasisMainWindowClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, // 移除 WS_EX_LAYERED，常规窗口即可
        L"StasisMainWindowClass", L"Stasis",
        WS_POPUP | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (hwnd)
    {
        EnableAcrylic(hwnd);
        // 创建标题字体
        g_hTitleFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SetWindowPos(hwnd, NULL, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

// 拖动相关
static BOOL g_bDragging = FALSE;
static POINT g_ptLastMouse;

// 绘制自定义标题栏
void DrawTitleBar(HDC hdc, HWND hwnd)
{
    RECT rcTitleBar = {0, 0, WINDOW_WIDTH, TITLE_BAR_HEIGHT};
    // 标题栏背景
    HBRUSH hTitleBg = CreateSolidBrush(RGB(20, 20, 30)); // 深蓝黑色
    FillRect(hdc, &rcTitleBar, hTitleBg);
    DeleteObject(hTitleBg);

    // 绘制图标 (16x16)
    HICON hIcon = LoadIconW(GetModuleHandleW(NULL), L"APP_ICON");
    DrawIconEx(hdc, 10, 10, hIcon, 16, 16, 0, NULL, DI_NORMAL);

    // 标题文字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(240, 240, 240));
    HFONT oldFont = SelectObject(hdc, g_hTitleFont);
    RECT rcText = {35, 0, 200, TITLE_BAR_HEIGHT};
    DrawTextW(hdc, L"Stasis", -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // 最小化按钮 (坐标在右侧)
    RECT rcMinBtn = {WINDOW_WIDTH - 70, 5, WINDOW_WIDTH - 40, TITLE_BAR_HEIGHT - 5};
    DrawRoundedButton(hdc, rcMinBtn, L"—", FALSE, FALSE);

    // 关闭按钮
    RECT rcCloseBtn = {WINDOW_WIDTH - 35, 5, WINDOW_WIDTH - 5, TITLE_BAR_HEIGHT - 5};
    DrawRoundedButton(hdc, rcCloseBtn, L"✕", FALSE, FALSE);
}

// 窗口过程
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        DebugLog(L"WM_CREATE called");
        // 创建 ListView (位置需要避开标题栏，标题栏高35px，客户区从35开始)
        HWND hList = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED,
            10, TITLE_BAR_HEIGHT + 10, WINDOW_WIDTH - 20, 220, hwnd, (HMENU)IDC_LISTVIEW, NULL, NULL);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
        g_State.hListView = hList;

        // 列
        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.cx = 60; lvc.pszText = L"状态"; ListView_InsertColumn(hList, 0, &lvc);
        lvc.cx = 130; lvc.pszText = L"进程名"; ListView_InsertColumn(hList, 1, &lvc);
        lvc.cx = 70; lvc.pszText = L"PID"; ListView_InsertColumn(hList, 2, &lvc);
        lvc.cx = 70; lvc.pszText = L"CPU%"; ListView_InsertColumn(hList, 3, &lvc);
        lvc.cx = 90; lvc.pszText = L"内存(KB)"; ListView_InsertColumn(hList, 4, &lvc);

        // 下方控件（调整Y坐标从 ListView 底部开始）
        int yBottom = TITLE_BAR_HEIGHT + 240;
        CreateWindowW(L"BUTTON", L"自动", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            10, yBottom + 10, 70, 28, hwnd, (HMENU)IDC_TOGGLE_AUTO, NULL, NULL);
        CreateWindowW(L"BUTTON", L"CPU", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            90, yBottom + 10, 90, 28, hwnd, (HMENU)IDC_SLIDER_CPU, NULL, NULL);
        CreateWindowW(L"BUTTON", L"内存", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            190, yBottom + 10, 90, 28, hwnd, (HMENU)IDC_SLIDER_MEM, NULL, NULL);
        CreateWindowW(L"BUTTON", L"解冻", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            290, yBottom + 10, 90, 28, hwnd, (HMENU)IDC_SLIDER_THAW, NULL, NULL);
        CreateWindowW(L"BUTTON", L"白名单", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            390, yBottom + 10, 70, 28, hwnd, (HMENU)IDC_BTN_WHITELIST, NULL, NULL);
        CreateWindowW(L"BUTTON", L"托盘", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            470, yBottom + 10, 60, 28, hwnd, (HMENU)IDC_BTN_TRAY, NULL, NULL);
        CreateWindowW(L"BUTTON", L"选项", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            540, yBottom + 10, 60, 28, hwnd, (HMENU)IDC_BTN_OPTIONS, NULL, NULL);

        // 反馈链接
        CreateWindowW(L"SysLink", L"<a href=\"https://github.com/sqxy090123/Stasis/issues\">遇到问题？提交反馈</a>",
            WS_CHILD | WS_VISIBLE, 10, yBottom + 50, 200, 20, hwnd, (HMENU)IDC_SYSLINK_FEEDBACK, NULL, NULL);

        // 子类化滑块
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

        // 设置定时器，每秒刷新进程列表和状态
        SetTimer(hwnd, 500, 1000, NULL);
        DebugLog(L"WM_CREATE completed");
        break;
    }
    case WM_TIMER:
        if (wParam == 500)
        {
            RefreshListView();
            InvalidateRect(hwnd, NULL, TRUE); // 触发状态栏重绘
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 先绘制标题栏
        DrawTitleBar(hdc, hwnd);

        // 客户区背景（深色半透明，配合亚克力）
        HBRUSH hBg = CreateSolidBrush(RGB(25, 25, 35));
        // 注意：从标题栏下开始填充
        RECT rcClientBelow = {0, TITLE_BAR_HEIGHT, rcClient.right, rcClient.bottom};
        FillRect(hdc, &rcClientBelow, hBg);
        DeleteObject(hBg);

        // 绘制状态栏 (在客户区顶部，标题栏之下)
        RECT rcStatusBar = {0, TITLE_BAR_HEIGHT, rcClient.right, TITLE_BAR_HEIGHT + 40};
        HBRUSH hStatusBg = CreateSolidBrush(RGB(18, 18, 28));
        FillRect(hdc, &rcStatusBar, hStatusBg);
        DeleteObject(hStatusBg);

        // 获取系统负载
        double cpu = GetTotalCpuUsage();
        double mem = GetTotalMemUsage();
        int frozen = 0;
        EnterCriticalSection(&g_State.cs);
        frozen = g_State.frozenCount;
        LeaveCriticalSection(&g_State.cs);

        // 字体
        HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(230, 230, 230));

        // CPU 进度条和文字
        RECT rcCpu = {10, TITLE_BAR_HEIGHT + 5, 200, TITLE_BAR_HEIGHT + 35};
        DrawGradientProgressBar(hdc, rcCpu, cpu, RGB(0,120,215), RGB(0,200,255));
        WCHAR cpuText[64];
        swprintf_s(cpuText, 64, L"CPU: %.1f%%", cpu);
        DrawTextW(hdc, cpuText, -1, &rcCpu, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 内存
        RECT rcMem = {210, TITLE_BAR_HEIGHT + 5, 400, TITLE_BAR_HEIGHT + 35};
        DrawGradientProgressBar(hdc, rcMem, mem, RGB(0,180,120), RGB(0,255,200));
        WCHAR memText[64];
        swprintf_s(memText, 64, L"内存: %.1f%%", mem);
        DrawTextW(hdc, memText, -1, &rcMem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 冻结数
        RECT rcFrozen = {410, TITLE_BAR_HEIGHT + 5, 600, TITLE_BAR_HEIGHT + 35};
        WCHAR frozenText[64];
        swprintf_s(frozenText, 64, L"已冻结: %d", frozen);
        DrawTextW(hdc, frozenText, -1, &rcFrozen, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_LBUTTONDOWN:
    {
        // 判断是否在标题栏区域（且不在按钮上）
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (pt.y < TITLE_BAR_HEIGHT)
        {
            // 检查是否在最小化/关闭按钮区域（略过）
            RECT rcMin = {WINDOW_WIDTH - 70, 0, WINDOW_WIDTH - 40, TITLE_BAR_HEIGHT};
            RECT rcClose = {WINDOW_WIDTH - 35, 0, WINDOW_WIDTH, TITLE_BAR_HEIGHT};
            if (PtInRect(&rcMin, pt) || PtInRect(&rcClose, pt))
                break; // 让按钮响应，不拖动
            g_bDragging = TRUE;
            SetCapture(hwnd);
            GetCursorPos(&g_ptLastMouse);
            DebugLog(L"Title bar drag start");
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (g_bDragging)
        {
            POINT ptCur;
            GetCursorPos(&ptCur);
            int dx = ptCur.x - g_ptLastMouse.x;
            int dy = ptCur.y - g_ptLastMouse.y;
            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            SetWindowPos(hwnd, NULL, rcWindow.left + dx, rcWindow.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            g_ptLastMouse = ptCur;
        }
        break;
    case WM_LBUTTONUP:
        if (g_bDragging)
        {
            ReleaseCapture();
            g_bDragging = FALSE;
            DebugLog(L"Title bar drag end");
        }
        break;
    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        switch (id)
        {
        case IDC_BTN_TRAY:
            ShowWindow(hwnd, SW_HIDE);
            g_State.trayVisible = TRUE;
            SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
            UpdateTrayIcon(g_State.frozenCount);
            break;
        case IDC_BTN_WHITELIST:
            MessageBoxW(hwnd, L"白名单编辑暂未实现", L"Stasis", MB_OK);
            break;
        case IDC_SYSLINK_FEEDBACK:
            ShellExecuteW(NULL, L"open", L"https://github.com/sqxy090123/Stasis/issues", NULL, NULL, SW_SHOWNORMAL);
            break;
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
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        g_State.trayVisible = TRUE;
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
        UpdateTrayIcon(g_State.frozenCount);
        // 气泡提示
        NOTIFYICONDATAW nid = {0};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_INFO;
        wcscpy_s(nid.szInfo, _countof(nid.szInfo), L"Stasis 已驻留后台");
        nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 500);
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