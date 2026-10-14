#pragma once
// 全局共享状态：所有模块都运行在同一 UI 线程，无需加锁。
#include <windows.h>
#include "config.h"

namespace app {

inline constexpr wchar_t kAppVersion[] = L"1.0";

inline HINSTANCE g_hInst = nullptr;
inline Config g_cfg;
inline HWND g_host = nullptr;   // 隐藏宿主窗口：托盘回调、钩子执行消息
inline HWND g_ui = nullptr;     // 设置窗口
inline bool g_hookFailed = false;
inline UINT WM_TASKBARCREATED = 0;  // RegisterWindowMessageW 结果

// 私有消息
inline constexpr UINT WM_APP_EXEC   = WM_APP + 1;  // wParam: XBUTTON1 / XBUTTON2
inline constexpr UINT WM_APP_TRAY   = WM_APP + 2;  // 托盘回调
inline constexpr UINT WM_APP_SHOWUI = WM_APP + 3;  // 第二实例请求显示设置

}  // namespace app
