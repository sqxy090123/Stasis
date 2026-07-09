// ui_controls.c - 自绘控件实现
#include "Stasis.h"

void DrawGradientProgressBar(HDC hdc, RECT rc, double value, COLORREF color1, COLORREF color2)
{
    if (value < 0.0) value = 0.0;
    if (value > 100.0) value = 100.0;
    int width = rc.right - rc.left;
    int fill = (int)(width * value / 100.0);

    // 背景
    HBRUSH hBack = CreateSolidBrush(RGB(60, 60, 60));
    FillRect(hdc, &rc, hBack);
    DeleteObject(hBack);

    // 渐变填充
    if (fill > 0)
    {
        for (int x = 0; x < fill; x++)
        {
            double ratio = (double)x / fill;
            int r = (int)(GetRValue(color1) * (1 - ratio) + GetRValue(color2) * ratio);
            int g = (int)(GetGValue(color1) * (1 - ratio) + GetGValue(color2) * ratio);
            int b = (int)(GetBValue(color1) * (1 - ratio) + GetBValue(color2) * ratio);
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
            HPEN oldPen = SelectObject(hdc, hPen);
            MoveToEx(hdc, rc.left + x, rc.top, NULL);
            LineTo(hdc, rc.left + x, rc.bottom);
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }
    }
    // 边框
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN oldPen = SelectObject(hdc, hPen);
    HBRUSH oldBr = SelectObject(hdc, nullBr);
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(hPen);
}

void DrawRoundedButton(HDC hdc, RECT rc, const WCHAR* text, BOOL hover, BOOL pressed)
{
    COLORREF bgColor = pressed ? RGB(80, 80, 80) : hover ? RGB(100, 100, 100) : RGB(70, 70, 70);
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    HBRUSH oldBr = SelectObject(hdc, hBrush);
    HPEN oldPen = SelectObject(hdc, hPen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawToggleSwitch(HDC hdc, RECT rc, BOOL state)
{
    HBRUSH hTrack = CreateSolidBrush(state ? RGB(0, 120, 215) : RGB(100, 100, 100));
    HBRUSH oldBr = SelectObject(hdc, hTrack);
    HPEN oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, rc.bottom - rc.top, rc.bottom - rc.top);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hTrack);

    int diameter = (rc.bottom - rc.top) - 4;
    int xPos = state ? rc.right - diameter - 2 : rc.left + 2;
    RECT circle = { xPos, rc.top + 2, xPos + diameter, rc.top + 2 + diameter };
    HBRUSH hCircle = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc, hCircle);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, circle.left, circle.top, circle.right, circle.bottom);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hCircle);
}

void DrawSlider(HDC hdc, RECT rc, int min, int max, int value)
{
    RECT track = { rc.left, rc.top + (rc.bottom - rc.top) / 3, rc.right, rc.bottom - (rc.bottom - rc.top) / 3 };
    HBRUSH hTrack = CreateSolidBrush(RGB(80, 80, 80));
    FillRect(hdc, &track, hTrack);
    DeleteObject(hTrack);

    double ratio = (double)(value - min) / (max - min);
    int fillWidth = (int)((rc.right - rc.left) * ratio);
    RECT fill = { rc.left, track.top, rc.left + fillWidth, track.bottom };
    HBRUSH hFill = CreateSolidBrush(RGB(0, 120, 215));
    FillRect(hdc, &fill, hFill);
    DeleteObject(hFill);

    int thumbX = rc.left + fillWidth - 5;
    RECT thumb = { thumbX, rc.top, thumbX + 10, rc.bottom };
    HBRUSH hThumb = CreateSolidBrush(RGB(200, 200, 200));
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    HPEN oldPen = SelectObject(hdc, hPen);
    HBRUSH oldBr = SelectObject(hdc, hThumb);
    RoundRect(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom, 4, 4);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hThumb);
    DeleteObject(hPen);
}

void InitCustomControls(void)
{
    // 初始化全局画笔、画刷（可在此添加）
}

void PaintCustomUI(HWND hwnd, HDC hdc)
{
    // 由 WM_DRAWITEM 分发，此处保留接口
}

// 滑块子类化过程
LRESULT CALLBACK SliderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    {
        SetCapture(hwnd);
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        RECT rc; GetClientRect(hwnd, &rc);
        int id = GetDlgCtrlID(hwnd);
        int *pVal = NULL, min = 0, max = 100;
        if (id == IDC_SLIDER_CPU) { min = 50; max = 100; pVal = &g_State.cpuThreshold; }
        else if (id == IDC_SLIDER_MEM) { min = 50; max = 100; pVal = &g_State.memThreshold; }
        else if (id == IDC_SLIDER_THAW) { min = 30; max = 80; pVal = &g_State.thawThreshold; }
        if (pVal)
        {
            double ratio = (double)pt.x / (rc.right - rc.left);
            *pVal = min + (int)(ratio * (max - min + 1));
            if (*pVal < min) *pVal = min;
            if (*pVal > max) *pVal = max;
            *pVal = ((*pVal - min) / 5) * 5 + min;
            InvalidateRect(hwnd, NULL, TRUE);
            SaveSettings();
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (GetCapture() == hwnd)
        {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            RECT rc; GetClientRect(hwnd, &rc);
            int id = GetDlgCtrlID(hwnd);
            int *pVal = NULL, min = 0, max = 100;
            if (id == IDC_SLIDER_CPU) { min = 50; max = 100; pVal = &g_State.cpuThreshold; }
            else if (id == IDC_SLIDER_MEM) { min = 50; max = 100; pVal = &g_State.memThreshold; }
            else if (id == IDC_SLIDER_THAW) { min = 30; max = 80; pVal = &g_State.thawThreshold; }
            if (pVal)
            {
                double ratio = (double)pt.x / (rc.right - rc.left);
                *pVal = min + (int)(ratio * (max - min + 1));
                if (*pVal < min) *pVal = min;
                if (*pVal > max) *pVal = max;
                *pVal = ((*pVal - min) / 5) * 5 + min;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        break;
    case WM_LBUTTONUP:
        ReleaseCapture();
        SaveSettings();
        break;
    }
    return CallWindowProc(oldProc, hwnd, msg, wParam, lParam);
}