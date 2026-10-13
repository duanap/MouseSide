#include "TrayUI.h"

#include "resource.h"

#include <iterator>
#include <shellapi.h>

namespace ms {
namespace tray {

namespace {

NOTIFYICONDATAW g_icon{};
HICON g_iconNormal = nullptr;
HICON g_iconPaused = nullptr;
bool g_added = false;

HICON LoadAppIcon(HINSTANCE hInst, int resourceId) {
    const int size = GetSystemMetrics(SM_CXSMICON);
    const HICON icon = reinterpret_cast<HICON>(
        LoadImageW(hInst, MAKEINTRESOURCEW(resourceId), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    if (icon) return icon;
    return LoadIconW(hInst, MAKEINTRESOURCEW(resourceId));
}

void ReleaseIcons() {
    if (g_iconNormal) { DestroyIcon(g_iconNormal); g_iconNormal = nullptr; }
    if (g_iconPaused) { DestroyIcon(g_iconPaused); g_iconPaused = nullptr; }
}

}  // namespace

bool Add(HWND hwnd, HINSTANCE hInst) {
    g_iconNormal = LoadAppIcon(hInst, IDI_APP);
    g_iconPaused = LoadAppIcon(hInst, IDI_APP_PAUSED);
    if (!g_iconNormal) {
        ReleaseIcons();
        return false;
    }

    ZeroMemory(&g_icon, sizeof(g_icon));
    g_icon.cbSize = sizeof(g_icon);
    g_icon.hWnd = hwnd;
    g_icon.uID = 1;
    g_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    g_icon.uCallbackMessage = WM_MS_TRAY;
    g_icon.hIcon = g_iconNormal;
    lstrcpynW(g_icon.szTip, kAppName, static_cast<int>(std::size(g_icon.szTip)));

    if (!Shell_NotifyIconW(NIM_ADD, &g_icon)) {
        ReleaseIcons();
        return false;
    }
    g_added = true;

    // 使用现代托盘行为（不显示"自定义通知"气泡样式）
    NOTIFYICONDATAW version = g_icon;
    version.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &version);
    return true;
}

void Remove() {
    if (g_added) {
        Shell_NotifyIconW(NIM_DELETE, &g_icon);
        g_added = false;
    }
    ReleaseIcons();
}

void Update(bool mappingEnabled) {
    if (!g_added) return;

    std::wstring tip = kAppName;
    tip += L"\n";
    tip += mappingEnabled ? L"映射已启用" : L"映射已暂停";
    lstrcpynW(g_icon.szTip, tip.c_str(), static_cast<int>(std::size(g_icon.szTip)));

    g_icon.uFlags = NIF_TIP | NIF_ICON;
    g_icon.hIcon = mappingEnabled ? g_iconNormal : g_iconPaused;
    Shell_NotifyIconW(NIM_MODIFY, &g_icon);
}

void ShowMenu(HWND hwnd) {
    const HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, kMenuOpen, L"打开设置");
    AppendMenuW(menu, MF_STRING, kMenuToggle, L"启用 / 暂停映射");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出程序");
    SetMenuDefaultItem(menu, kMenuOpen, FALSE);

    POINT pt{};
    GetCursorPos(&pt);

    // TrackPopupMenu 需要前台窗口，否则菜单不会自动关闭
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

}  // namespace tray
}  // namespace ms
