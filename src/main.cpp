#include <windows.h>
#include <commctrl.h>

#include "appfuncs.h"
#include "appstate.h"
#include "config.h"
#include "dbglog.h"
#include "hook.h"
#include "mapper.h"
#include "tray.h"
#include "ui.h"

namespace {

constexpr wchar_t kHostClass[] = L"MouseSideHost";
// 与 tray.cpp ShowMenu 中的菜单命令对应
constexpr UINT TRAY_OPEN = 1001;
constexpr UINT TRAY_TOGGLE = 1002;
constexpr UINT TRAY_EXIT = 1003;

LRESULT CALLBACK HostProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == app::WM_TASKBARCREATED) {  // explorer 重启后恢复托盘图标
        tray::Init();
        return 0;
    }
    switch (msg) {
        case app::WM_APP_TRAY:
            tray::OnCallback(lp);
            return 0;
        case app::WM_APP_EXEC: {
            const ButtonConfig& bc =
                (wp == XBUTTON1) ? app::g_cfg.btn4 : app::g_cfg.btn5;
            MsDbg("exec xb=%u", static_cast<unsigned>(wp));
            mapper::Execute(bc);
            return 0;
        }
        case app::WM_APP_SHOWUI:
            app::ShowSettings();
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case TRAY_OPEN: app::ShowSettings(); break;
                case TRAY_TOGGLE: app::SetEnabled(!app::g_cfg.enabled); break;
                case TRAY_EXIT: app::ExitApp(); break;
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            hook::Uninstall();
            tray::Remove();
            SaveConfig(app::g_cfg);
            if (app::g_ui) DestroyWindow(app::g_ui);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

namespace app {

void SetEnabled(bool on) {
    app::g_cfg.enabled = on;
    SaveConfig(app::g_cfg);
    tray::UpdateIcon();
    ui::Refresh();
}

void ShowSettings() {
    ui::Show();
}

void ExitApp() {
    if (app::g_host) DestroyWindow(app::g_host);
}

}  // namespace app

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR lpCmdLine, int) {
    app::g_hInst = hInst;

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\MouseSide.Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // 唤醒已有实例的设置窗口，而不是创建第二个监听进程
        const HWND prev = FindWindowW(kHostClass, nullptr);
        if (prev) PostMessageW(prev, app::WM_APP_SHOWUI, 0, 0);
        if (mutex) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
        return 0;
    }

    const bool firstRun = !LoadConfig(&app::g_cfg);
    if (firstRun) SaveConfig(app::g_cfg);
    ApplyAutostart(app::g_cfg.autostart);

    WNDCLASSW hc = {};
    hc.lpfnWndProc = HostProc;
    hc.hInstance = hInst;
    hc.lpszClassName = kHostClass;
    RegisterClassW(&hc);
    app::g_host = CreateWindowExW(0, kHostClass, L"MouseSide", WS_OVERLAPPED,
                                  0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

    const INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    app::WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    tray::Init();
    if (!hook::Install()) {
        app::g_hookFailed = true;
        MessageBoxW(nullptr,
                    L"鼠标监听初始化失败，侧键映射不可用。\n"
                    L"可能是其他程序占用了全局鼠标钩子。",
                    L"MouseSide", MB_OK | MB_ICONWARNING);
    }
    ui::CreateAll();

    const bool launchedToTray = lpCmdLine && wcsstr(lpCmdLine, L"/tray") != nullptr;
    if (firstRun && !launchedToTray) app::ShowSettings();  // 首次运行展示一次设置窗口

    // 初始化完成后修剪工作集，降低常驻内存（页面可按需换回）
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1),
                             static_cast<SIZE_T>(-1));

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (app::g_ui && IsDialogMessageW(app::g_ui, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return 0;
}
