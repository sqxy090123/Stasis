// ui_controls.c - 自绘控件实现
#include "Stasis.h"

void DrawGradientProgressBar(HDC hdc, RECT rc, double value, COLORREF color1, COLORREF color2)
{
    // 双色渐变条，省略具体实现，使用FillRect渐变模拟
    HBRUSH hBrush = CreateSolidBrush(color1);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);
}

void DrawRoundedButton(HDC hdc, RECT rc, const WCHAR* text, BOOL hover, BOOL pressed)
{
    // 圆角矩形按钮绘制
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawToggleSwitch(HDC hdc, RECT rc, BOOL state)
{
    // 滑动开关
}

void DrawSlider(HDC hdc, RECT rc, int min, int max, int value)
{
    // 滑块
}

void InitCustomControls(void) { /* 预加载笔刷字体等 */ }

void PaintCustomUI(HWND hwnd, HDC hdc)
{
    // 标题栏、状态栏、渐变条等绘制
    RECT topBar = {0, 0, 700, 80};
    DrawGradientProgressBar(hdc, topBar, 0.5, RGB(255,165,0), RGB(0,0,255));
}