// app_window.c - 现代化主窗口，自绘标题栏，清晰字体，定时刷新
#include "Stasis.h"

AppState g_State = {0};

// 亚克力效果
typedef struct _ACCENTPOLICY { DWORD AccentState; DWORD AccentFlags; DWORD GradientColor; DWORD AnimationId; } ACCENTPOLICY;
typedef struct _WINDOWCOMPOSITIONATTRIBDATA { DWORD Attrib; PVOID pvData; SIZE_T cbData; } WINDOWCOMPOSITIONATTRIBDATA;
#define WCA_ACCENT_POLICY 19
#define ACCENT_ENABLE_BLURBEHIND 3
typedef BOOL (WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
static pSetWindowCompositionAttribute SetWindowCompositionAttribute = NULL;

static void EnableAcrylic(HWND hwnd) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        SetWindowCompositionAttribute = (pSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (SetWindowCompositionAttribute) {
            ACCENTPOLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, 0, 0 };
            WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };
            SetWindowCompositionAttribute(hwnd, &data);
            g_State.acrylicEnabled = TRUE;
        }
    }
}

static HFONT g_hTitleFont = NULL;
static HFONT g_hBtnFont = NULL;
static HFONT g_hListFont = NULL;
static HFONT g_hStatusFont = NULL;

// DPI缩放因子
static float g_dpiScale = 1.0f;

static void RebuildFonts(void) {
    if (g_hTitleFont) DeleteObject(g_hTitleFont);
    if (g_hBtnFont) DeleteObject(g_hBtnFont);
    if (g_hListFont) DeleteObject(g_hListFont);
    if (g_hStatusFont) DeleteObject(g_hStatusFont);
    int titleFontSize = (int)(16 * g_dpiScale);
    int btnFontSize = (int)(13 * g_dpiScale);
    int listFontSize = (int)(14 * g_dpiScale);
    int statusFontSize = (int)(15 * g_dpiScale);
    g_hTitleFont = CreateFontW(-titleFontSize, 0,0,0, FW_SEMIBOLD, FALSE,FALSE,FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
    g_hBtnFont = CreateFontW(-btnFontSize, 0,0,0, FW_NORMAL, FALSE,FALSE,FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
    g_hListFont = CreateFontW(-listFontSize, 0,0,0, FW_NORMAL, FALSE,FALSE,FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
    g_hStatusFont = CreateFontW(-statusFontSize, 0,0,0, FW_NORMAL, FALSE,FALSE,FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
}

static void UpdateDpiScale(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi == 0) dpi = 96;
    g_dpiScale = dpi / 96.0f;
    RebuildFonts();
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND CreateMainWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, MainWndProc, 0, 0, hInstance,
        LoadIconW(hInstance, L"APP_ICON"), LoadCursorW(NULL, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH),
        NULL, L"StasisMainWindowClass" };
    RegisterClassExW(&wc);

    // 获取DPI缩放 - 使用 GetDpiForSystem 替代不可靠的 GetDpiForWindow(NULL)
    UINT dpi = GetDpiForSystem();
    if (dpi == 0) {
        HDC hdcScreen = GetDC(NULL);
        dpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
        ReleaseDC(NULL, hdcScreen);
    }
    g_dpiScale = dpi / 96.0f;

    int winW = (int)(700 * g_dpiScale);
    int winH = (int)(500 * g_dpiScale);

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"StasisMainWindowClass", L"Stasis",
        WS_POPUP | WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        NULL, NULL, hInstance, NULL);
    if (hwnd) {
        EnableAcrylic(hwnd);
        RebuildFonts();

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

// 标题栏绘制
static void DrawTitleBar(HDC hdc, HWND hwnd) {
    RECT rc = {0,0, (int)(700*g_dpiScale), (int)(35*g_dpiScale) };
    HBRUSH hBr = CreateSolidBrush(RGB(20,20,30));
    FillRect(hdc, &rc, hBr);
    DeleteObject(hBr);

    // 图标
    HICON hIcon = LoadIconW(GetModuleHandleW(NULL), L"APP_ICON");
    DrawIconEx(hdc, 10, (int)(10*g_dpiScale), hIcon, (int)(16*g_dpiScale), (int)(16*g_dpiScale), 0, NULL, DI_NORMAL);

    // 标题文字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(240,240,240));
    HFONT oldFont = SelectObject(hdc, g_hTitleFont);
    RECT rcText = { (int)(35*g_dpiScale), 0, (int)(200*g_dpiScale), rc.bottom };
    DrawTextW(hdc, L"Stasis", -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // 最小化、关闭按钮区域（由自绘处理，此处仅绘制按钮）
    RECT rcMin = { (int)(700*g_dpiScale) - (int)(70*g_dpiScale), (int)(5*g_dpiScale),
                   (int)(700*g_dpiScale) - (int)(40*g_dpiScale), (int)(30*g_dpiScale) };
    DrawRoundedButton(hdc, rcMin, L"—", FALSE, FALSE);
    RECT rcClose = { (int)(700*g_dpiScale) - (int)(35*g_dpiScale), (int)(5*g_dpiScale),
                     (int)(700*g_dpiScale) - (int)(5*g_dpiScale), (int)(30*g_dpiScale) };
    DrawRoundedButton(hdc, rcClose, L"✕", FALSE, FALSE);
}

// 主窗口过程
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL dragging = FALSE;
    static POINT lastPt;
    switch (msg) {
    case WM_CREATE: {
        DebugLog(L"WM_CREATE");
        int yTitle = (int)(35*g_dpiScale);
        int winW = (int)(700*g_dpiScale);
        // ListView (无 LVS_OWNERDRAWFIXED，正常绘制)
        HWND hList = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            (int)(10*g_dpiScale), yTitle + (int)(10*g_dpiScale),
            winW - (int)(20*g_dpiScale), (int)(220*g_dpiScale),
            hwnd, (HMENU)IDC_LISTVIEW, NULL, NULL);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
        SendMessage(hList, WM_SETFONT, (WPARAM)g_hListFont, TRUE);
        g_State.hListView = hList;

        LVCOLUMNW lvc = { LVCF_TEXT | LVCF_WIDTH };
        lvc.cx = (int)(60*g_dpiScale); lvc.pszText = L"状态"; ListView_InsertColumn(hList, 0, &lvc);
        lvc.cx = (int)(130*g_dpiScale); lvc.pszText = L"进程名"; ListView_InsertColumn(hList, 1, &lvc);
        lvc.cx = (int)(70*g_dpiScale); lvc.pszText = L"PID"; ListView_InsertColumn(hList, 2, &lvc);
        lvc.cx = (int)(70*g_dpiScale); lvc.pszText = L"CPU%"; ListView_InsertColumn(hList, 3, &lvc);
        lvc.cx = (int)(90*g_dpiScale); lvc.pszText = L"内存(KB)"; ListView_InsertColumn(hList, 4, &lvc);

        int yBtn = yTitle + (int)(240*g_dpiScale);
        int btnH = (int)(28*g_dpiScale);
        // 创建自绘按钮（文本已设置）
        HWND btnAuto = CreateWindowW(L"BUTTON", L"自动", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(10*g_dpiScale), yBtn, (int)(70*g_dpiScale), btnH, hwnd, (HMENU)IDC_TOGGLE_AUTO, NULL, NULL);
        SendMessage(btnAuto, WM_SETFONT, (WPARAM)g_hBtnFont, TRUE);
        // ... 其他按钮类似 ...
        CreateWindowW(L"BUTTON", L"CPU", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(90*g_dpiScale), yBtn, (int)(90*g_dpiScale), btnH, hwnd, (HMENU)IDC_SLIDER_CPU, NULL, NULL);
        CreateWindowW(L"BUTTON", L"内存", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(190*g_dpiScale), yBtn, (int)(90*g_dpiScale), btnH, hwnd, (HMENU)IDC_SLIDER_MEM, NULL, NULL);
        CreateWindowW(L"BUTTON", L"解冻", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(290*g_dpiScale), yBtn, (int)(90*g_dpiScale), btnH, hwnd, (HMENU)IDC_SLIDER_THAW, NULL, NULL);
        CreateWindowW(L"BUTTON", L"白名单", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(390*g_dpiScale), yBtn, (int)(70*g_dpiScale), btnH, hwnd, (HMENU)IDC_BTN_WHITELIST, NULL, NULL);
        CreateWindowW(L"BUTTON", L"托盘", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(470*g_dpiScale), yBtn, (int)(60*g_dpiScale), btnH, hwnd, (HMENU)IDC_BTN_TRAY, NULL, NULL);
        CreateWindowW(L"BUTTON", L"选项", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            (int)(540*g_dpiScale), yBtn, (int)(60*g_dpiScale), btnH, hwnd, (HMENU)IDC_BTN_OPTIONS, NULL, NULL);
        // 反馈链接
        CreateWindowW(L"SysLink", L"<a href=\"https://github.com/sqxy090123/Stasis/issues\">提交反馈</a>",
            WS_CHILD|WS_VISIBLE, (int)(10*g_dpiScale), yBtn + (int)(40*g_dpiScale), (int)(200*g_dpiScale), (int)(20*g_dpiScale),
            hwnd, (HMENU)IDC_SYSLINK_FEEDBACK, NULL, NULL);

        // 子类化滑块
        SetWindowSubclass(GetDlgItem(hwnd, IDC_SLIDER_CPU), SliderSubclassProc, IDC_SLIDER_CPU, 0);
        SetWindowSubclass(GetDlgItem(hwnd, IDC_SLIDER_MEM), SliderSubclassProc, IDC_SLIDER_MEM, 0);
        SetWindowSubclass(GetDlgItem(hwnd, IDC_SLIDER_THAW), SliderSubclassProc, IDC_SLIDER_THAW, 0);

        CreateTrayIcon(hwnd);
        ApplySettingsToUI();
        SetTimer(hwnd, 500, 1000, NULL);  // 刷新定时器
        break;
    }
    case WM_TIMER:
        if (wParam == 500) {
            static double lastCpu = -1.0, lastMem = -1.0;
            static int lastFrozen = -1;
            double cpu = GetTotalCpuUsage();
            double mem = GetTotalMemUsage();
            int frozen = 0;
            EnterCriticalSection(&g_State.cs);
            frozen = g_State.frozenCount;
            LeaveCriticalSection(&g_State.cs);
            int cpuI = (int)cpu, memI = (int)mem;
            int lastCpuI = (int)lastCpu, lastMemI = (int)lastMem;
            if (cpuI != lastCpuI || memI != lastMemI || frozen != lastFrozen) {
                lastCpu = cpu; lastMem = mem;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (frozen != lastFrozen) {
                lastFrozen = frozen;
                RefreshListView();
            }
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        // 背景（覆盖整个客户区）
        HBRUSH hBg = CreateSolidBrush(RGB(25,25,35));
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);

        // 标题栏
        DrawTitleBar(hdc, hwnd);

        // 状态栏（在标题下方）
        int yTitle = (int)(35*g_dpiScale);
        RECT rcStatus = { 0, yTitle, rc.right, yTitle + (int)(40*g_dpiScale) };
        HBRUSH hStatusBg = CreateSolidBrush(RGB(18,18,28));
        FillRect(hdc, &rcStatus, hStatusBg);
        DeleteObject(hStatusBg);

        double cpu = GetTotalCpuUsage();
        double mem = GetTotalMemUsage();
        int frozen = 0;
        EnterCriticalSection(&g_State.cs);
        frozen = g_State.frozenCount;
        LeaveCriticalSection(&g_State.cs);

        HFONT oldF = SelectObject(hdc, g_hStatusFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(230,230,230));

        RECT rcCpu = { (int)(10*g_dpiScale), yTitle+5, (int)(200*g_dpiScale), yTitle+35 };
        DrawGradientProgressBar(hdc, rcCpu, cpu, RGB(0,120,215), RGB(0,200,255));
        WCHAR text[64];
        swprintf_s(text, 64, L"CPU: %.1f%%", cpu);
        DrawTextW(hdc, text, -1, &rcCpu, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

        RECT rcMem = { (int)(210*g_dpiScale), yTitle+5, (int)(400*g_dpiScale), yTitle+35 };
        DrawGradientProgressBar(hdc, rcMem, mem, RGB(0,180,120), RGB(0,255,200));
        swprintf_s(text, 64, L"内存: %.1f%%", mem);
        DrawTextW(hdc, text, -1, &rcMem, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

        RECT rcFz = { (int)(410*g_dpiScale), yTitle+5, (int)(600*g_dpiScale), yTitle+35 };
        swprintf_s(text, 64, L"已冻结: %d", frozen);
        DrawTextW(hdc, text, -1, &rcFz, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

        SelectObject(hdc, oldF);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_ERASEBKGND: return TRUE;
    case WM_DPICHANGED: {
        UpdateDpiScale(hwnd);
        RECT* rcNew = (RECT*)lParam;
        SetWindowPos(hwnd, NULL,
            rcNew->left, rcNew->top,
            rcNew->right - rcNew->left, rcNew->bottom - rcNew->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (pt.y < (int)(35*g_dpiScale)) {
            // 忽略按钮区域
            RECT rcMin = { (int)(700*g_dpiScale)-70,0, (int)(700*g_dpiScale)-40, (int)(35*g_dpiScale) };
            RECT rcClose = { (int)(700*g_dpiScale)-35,0, (int)(700*g_dpiScale), (int)(35*g_dpiScale) };
            if (!PtInRect(&rcMin, pt) && !PtInRect(&rcClose, pt)) {
                dragging = TRUE;
                SetCapture(hwnd);
                GetCursorPos(&lastPt);
            }
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (dragging) {
            POINT cur; GetCursorPos(&cur);
            int dx = cur.x - lastPt.x, dy = cur.y - lastPt.y;
            RECT wrc; GetWindowRect(hwnd, &wrc);
            SetWindowPos(hwnd, NULL, wrc.left+dx, wrc.top+dy, 0,0, SWP_NOSIZE|SWP_NOZORDER);
            lastPt = cur;
        }
        break;
    case WM_LBUTTONUP:
        if (dragging) { ReleaseCapture(); dragging = FALSE; }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_TRAY:
            ShowWindow(hwnd, SW_HIDE);
            g_State.trayVisible = TRUE;
            SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
            UpdateTrayIcon(g_State.frozenCount);
            break;
        case IDC_SYSLINK_FEEDBACK:
            ShellExecuteW(NULL, L"open", L"https://github.com/sqxy090123/Stasis/issues", NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        break;
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) ShowTrayMenu(hwnd);
        else if (lParam == WM_LBUTTONDBLCLK) {
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
        NOTIFYICONDATAW nid = { sizeof(NOTIFYICONDATAW), hwnd, 1, NIF_INFO };
        wcscpy_s(nid.szInfo, L"Stasis 已驻留后台");
        Shell_NotifyIconW(NIM_MODIFY, &nid);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 500);
        DeleteObject(g_hTitleFont);
        DeleteObject(g_hBtnFont);
        DeleteObject(g_hListFont);
        DeleteObject(g_hStatusFont);
        PostQuitMessage(0);
        break;
    default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void RefreshListView(void) {
    HWND hList = g_State.hListView;
    if (!hList) return;
    ListView_DeleteAllItems(hList);
    DWORD pids[1024]; int count;
    EnumProcessesEx(pids, 1024, &count);
    for (int i=0; i<count; i++) {
        DWORD pid = pids[i];
        WCHAR name[MAX_PATH]=L"<unknown>";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ, FALSE, pid);
        if (hProc) {
            WCHAR path[MAX_PATH]; DWORD sz=MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, path, &sz)) {
                WCHAR* fname = wcsrchr(path, L'\\');
                wcscpy_s(name, MAX_PATH, fname ? fname+1 : path);
            }
            CloseHandle(hProc);
        }
        BOOL frozen = FALSE;
        EnterCriticalSection(&g_State.cs);
        for (int j=0; j<g_State.frozenCount; j++) if (g_State.frozenStack[j]==pid) { frozen=TRUE; break; }
        LeaveCriticalSection(&g_State.cs);

        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = frozen ? ICON_FROZEN : ICON_RUNNING;
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, name);
        WCHAR buf[32];
        swprintf_s(buf, 32, L"%lu", pid); ListView_SetItemText(hList, i, 2, buf);
        ListView_SetItemText(hList, i, 3, L"0.0");
        SIZE_T memKB=0; GetProcessMemoryKB(pid, &memKB);
        swprintf_s(buf, 32, L"%llu", (unsigned long long)memKB);
        ListView_SetItemText(hList, i, 4, buf);
    }
}

void UpdateUIFromState() { InvalidateRect(g_State.hMainWnd, NULL, TRUE); }