// Utils.h
//
// 少量界面通用工具（头文件内联，避免为几个小函数单独建一个 .cpp）。
//
// 里面刻意通过 GetProcAddress 动态获取 GetDpiForWindow，
// 这样即使使用较旧的 Windows SDK 头文件也能正常编译。

#pragma once

#include <windows.h>

namespace ms {
namespace ui {

// 窗口所在显示器的 DPI（失败时回落到 96）
inline UINT GetDpi(HWND window) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

    static GetDpiForWindowFn resolve = reinterpret_cast<GetDpiForWindowFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                               "GetDpiForWindow")));

    if (resolve != nullptr) {
        const UINT dpi = resolve(window);
        if (dpi != 0) {
            return dpi;
        }
    }

    HDC screenDc = GetDC(nullptr);
    int dpi = 96;
    if (screenDc != nullptr) {
        dpi = GetDeviceCaps(screenDc, LOGPIXELSX);
        ReleaseDC(nullptr, screenDc);
    }
    return static_cast<UINT>(dpi > 0 ? dpi : 96);
}

// 按 DPI 缩放逻辑像素值
inline int Scale(HWND window, int value) {
    return MulDiv(value, static_cast<int>(GetDpi(window)), 96);
}

// 创建界面字体（默认使用中文界面下表现更好的微软雅黑 UI）
inline HFONT CreateUiFont(HWND window, int pointSize, int weight) {
    const int height = -MulDiv(pointSize, static_cast<int>(GetDpi(window)), 72);
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET,
                       OUT_TT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Microsoft YaHei UI");
}

// 把窗口居中到 owner（owner 不可用时居中到主屏工作区）
inline void CenterWindow(HWND window, HWND owner) {
    RECT outer{};
    if (!GetWindowRect(window, &outer)) {
        return;
    }
    const int width  = outer.right - outer.left;
    const int height = outer.bottom - outer.top;

    RECT anchor{};
    if (owner == nullptr || !IsWindowVisible(owner) || !GetWindowRect(owner, &anchor)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    }

    const int x = anchor.left + ((anchor.right - anchor.left) - width) / 2;
    const int y = anchor.top + ((anchor.bottom - anchor.top) - height) / 2;

    SetWindowPos(window, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace ui
}  // namespace ms
