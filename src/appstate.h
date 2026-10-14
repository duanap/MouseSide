#pragma once
// 全局共享状态。
// 配置（g_cfg）只在 UI 线程读写；钩子线程绝不直接访问 g_cfg，
// 而是通过 LoadSnapshot() 读取 UI 线程发布的快照，避免数据竞争。
#include <windows.h>
#include "config.h"

namespace app {

inline constexpr wchar_t kAppVersion[] = L"1.0";

inline HINSTANCE g_hInst = nullptr;
inline Config g_cfg;          // 仅 UI 线程访问
inline HWND g_host = nullptr; // 隐藏宿主窗口：托盘回调、钩子执行消息
inline HWND g_ui = nullptr;   // 设置窗口
inline bool g_hookFailed = false;
inline UINT WM_TASKBARCREATED = 0;  // RegisterWindowMessageW 结果

// 私有消息
inline constexpr UINT WM_APP_EXEC   = WM_APP + 1;  // wParam: XBUTTON1 / XBUTTON2
inline constexpr UINT WM_APP_TRAY   = WM_APP + 2;  // 托盘回调
inline constexpr UINT WM_APP_SHOWUI = WM_APP + 3;  // 第二实例请求显示设置

// ---- 钩子线程读取的配置快照 ----
// SRWLOCK 只保护这约 100 字节的拷贝，锁内不做任何 I/O，
// 钩子回调侧的等待是微秒级，不会触发低级钩子超时。
struct HookSnapshot {
    bool enabled = false;
    ButtonConfig btn[2];  // [0]=XBUTTON1 [1]=XBUTTON2
};
inline SRWLOCK g_snapLock = SRWLOCK_INIT;
inline HookSnapshot g_snap = {};

// UI 线程在 g_cfg 发生变化后调用（加载、启用切换、映射修改、恢复默认）
inline void PublishSnapshot() {
    AcquireSRWLockExclusive(&g_snapLock);
    g_snap.enabled = g_cfg.enabled;
    g_snap.btn[0] = g_cfg.btn4;
    g_snap.btn[1] = g_cfg.btn5;
    ReleaseSRWLockExclusive(&g_snapLock);
}

// 钩子线程调用
inline HookSnapshot LoadSnapshot() {
    HookSnapshot s;
    AcquireSRWLockShared(&g_snapLock);
    s = g_snap;
    ReleaseSRWLockShared(&g_snapLock);
    return s;
}

}  // namespace app
