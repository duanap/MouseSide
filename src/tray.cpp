#include "tray.h"

#include <shellapi.h>

#include "appstate.h"

namespace {

NOTIFYICONDATAW g_nid = {};
bool g_added = false;

HICON StateIcon() {
    UINT id = app::g_cfg.enabled ? 1 : 2;  // 资源：1=正常，2=已暂停
    HICON h = LoadIconW(app::g_hInst, MAKEINTRESOURCEW(id));
    return h ? h : LoadIconW(nullptr, IDI_APPLICATION);
}

void SetTip(wchar_t* dst) {
    lstrcpynW(dst, app::g_cfg.enabled ? L"MouseSide（映射已启用）"
                                      : L"MouseSide（映射已暂停）", 128);
}

void ShowMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1001, L"打开设置(&O)");
    AppendMenuW(m, MF_STRING | (app::g_cfg.enabled ? MF_CHECKED : MF_UNCHECKED),
                1002, L"启用映射(&E)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 1003, L"退出(&X)");
    SetForegroundWindow(app::g_host);  // 让菜单在点击外部时正常收起
    TrackPopupMenuEx(m, TPM_RIGHTBUTTON, pt.x, pt.y, app::g_host, nullptr);
    PostMessageW(app::g_host, WM_NULL, 0, 0);
    DestroyMenu(m);
}

}  // namespace

namespace tray {

bool Init() {
    g_nid = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = app::g_host;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = app::WM_APP_TRAY;
    g_nid.hIcon = StateIcon();
    SetTip(g_nid.szTip);
    g_added = Shell_NotifyIconW(NIM_ADD, &g_nid) != FALSE;
    return g_added;
}

void UpdateIcon() {
    if (!g_added) return;
    g_nid.hIcon = StateIcon();
    SetTip(g_nid.szTip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void Remove() {
    if (g_added) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_added = false;
    }
}

void ShowBalloon(const wchar_t* title, const wchar_t* msg) {
    if (!g_added) return;
    NOTIFYICONDATAW nid = g_nid;
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_WARNING;
    lstrcpynW(nid.szInfoTitle, title, 64);
    lstrcpynW(nid.szInfo, msg, 256);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void OnCallback(LPARAM lParam) {
    switch (lParam) {
        case WM_LBUTTONDBLCLK:
            ShowWindow(app::g_ui, SW_RESTORE);
            SetForegroundWindow(app::g_ui);
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowMenu();
            break;
        default:
            break;
    }
}

}  // namespace tray
