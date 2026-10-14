#include "hook.h"

#include "appstate.h"
#include "dbglog.h"

namespace {

HHOOK g_hook = nullptr;
HANDLE g_thread = nullptr;
DWORD g_threadId = 0;
HANDLE g_ready = nullptr;

// 仅钩子线程访问：记录“本次按下”是否被拦截。
// 按下与松开之间配置可能变化（如暂停映射），松开必须沿用按下时的决定，
// 否则会出现按下被吞、松开放行（或相反）的不对称。
bool g_intercepted[2] = {false, false};

// 低级钩子回调必须保持轻量：只读快照、记录状态、PostMessage。
// 模拟键盘在主线程执行。
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_XBUTTONDOWN || wParam == WM_XBUTTONUP) {
            const UINT xb = HIWORD(info->mouseData);  // XBUTTON1=1 / XBUTTON2=2
            if (xb == XBUTTON1 || xb == XBUTTON2) {
                const int idx = (xb == XBUTTON1) ? 0 : 1;
                if (wParam == WM_XBUTTONDOWN) {
                    const app::HookSnapshot snap = app::LoadSnapshot();
                    g_intercepted[idx] =
                        snap.enabled && snap.btn[idx].type != ActionType::Passthrough;
                    if (g_intercepted[idx]) {
                        // 仅按下时执行一次，松开只拦截
                        if (PostMessageW(app::g_host, app::WM_APP_EXEC, xb, 0))
                            return 1;  // 吞掉原始前进/后退
                        g_intercepted[idx] = false;  // 投递失败 → 放行原始事件
                    }
                } else if (g_intercepted[idx]) {
                    g_intercepted[idx] = false;
                    return 1;  // 与按下同命运：对应的松开也吞掉
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// 钩子专用线程：回调不能跑在 UI 线程上——界面卡顿会让回调超时，
// Windows 会把超时的低级钩子静默移出钩子链，导致映射失效。
DWORD WINAPI HookThreadMain(LPVOID) {
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
    SetEvent(g_ready);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
    return 0;
}

}  // namespace

namespace hook {

bool Install() {
    if (g_thread) return g_hook != nullptr;
    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, HookThreadMain, nullptr, 0, &g_threadId);
    if (!g_thread) return false;
    WaitForSingleObject(g_ready, 2000);
    return g_hook != nullptr;
}

void Uninstall() {
    if (g_thread) {
        PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_ready) {
        CloseHandle(g_ready);
        g_ready = nullptr;
    }
}

bool Installed() { return g_hook != nullptr; }

}  // namespace hook
