#include "MouseHook.h"

namespace ms {
namespace mousehook {

namespace {

HHOOK g_hook = nullptr;
HWND g_target = nullptr;
bool g_enabled = false;
Mapping g_mappings[2]{};
// 记录该键的按下事件是否被拦截，保证对应的松开事件也被拦截，
// 避免浏览器收到"孤儿" up 事件。
bool g_blocked[2] = {false, false};

bool ShouldBlock(int index) {
    if (!g_enabled) return false;
    return g_mappings[index].kind != ActionKind::Original;
}

int ButtonIndexFromData(DWORD mouseData) {
    const WORD button = HIWORD(mouseData);
    if (button == XBUTTON1) return 0;
    if (button == XBUTTON2) return 1;
    return -1;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);

        bool handled = false;
        switch (wParam) {
            case WM_XBUTTONDOWN:
            case WM_NCXBUTTONDOWN: {
                const int index = ButtonIndexFromData(info->mouseData);
                if (index >= 0) {
                    const bool block = ShouldBlock(index);
                    g_blocked[index] = block;
                    if (block && g_target) {
                        // 钩子回调保持轻量：只投递消息，由窗口过程执行模拟输入
                        PostMessageW(g_target, WM_MS_EXEC_MAPPING, static_cast<WPARAM>(index), 0);
                    }
                    handled = block;
                }
                break;
            }
            case WM_XBUTTONUP:
            case WM_NCXBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_NCXBUTTONDBLCLK: {
                const int index = ButtonIndexFromData(info->mouseData);
                if (index >= 0 && g_blocked[index]) {
                    if (wParam == WM_XBUTTONUP || wParam == WM_NCXBUTTONUP) g_blocked[index] = false;
                    handled = true;  // 吞掉，避免重复触发与原始前进/后退
                }
                break;
            }
            default:
                break;
        }
        if (handled) return 1;
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

}  // namespace

bool Install(HWND target) {
    if (g_hook) return true;
    g_target = target;
    g_blocked[0] = g_blocked[1] = false;
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
    return g_hook != nullptr;
}

void Uninstall() {
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
    g_blocked[0] = g_blocked[1] = false;
}

bool Installed() {
    return g_hook != nullptr;
}

void SetEnabled(bool enabled) {
    g_enabled = enabled;
    g_blocked[0] = g_blocked[1] = false;
}

void SetMappings(const Mapping& m4, const Mapping& m5) {
    g_mappings[0] = m4;
    g_mappings[1] = m5;
}

}  // namespace mousehook
}  // namespace ms
