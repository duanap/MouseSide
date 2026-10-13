// MouseSide - 程序入口：单实例、消息循环、隐藏主窗口
#include "Common.h"

#include <commctrl.h>
#include "Config.h"
#include "KeyMapper.h"
#include "MouseHook.h"
#include "SettingsWindow.h"
#include "TrayUI.h"
#include "resource.h"

#ifdef __GNUC__
// GetProcAddress 返回通用函数指针，转换时 GCC 会警告，这里属于预期用法
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

namespace ms {

namespace {

AppConfig g_config{};
HINSTANCE g_hInst = nullptr;
HWND g_hidden = nullptr;
UINT g_taskbarCreated = 0;

bool CommandLineHas(PWSTR cmdLine, const wchar_t* arg) {
    if (!cmdLine || !arg) return false;
    return wcsstr(cmdLine, arg) != nullptr;
}

void EnablePerMonitorDpi() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    using CtxFn = BOOL(WINAPI*)(HANDLE);
    auto setCtx = reinterpret_cast<CtxFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setCtx) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
        setCtx(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)));
        return;
    }
    auto setAware = reinterpret_cast<BOOL(WINAPI*)(void)>(
        GetProcAddress(user32, "SetProcessDPIAware"));
    if (setAware) setAware();
}

void QuitApp() {
    tray::Remove();
    mousehook::Uninstall();
    settings::CloseAll();
    PostQuitMessage(0);
}

LRESULT CALLBACK HiddenProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_taskbarCreated && msg == g_taskbarCreated) {
        if (!tray::Restore()) {
            settings::Show(hwnd);
            MessageBoxW(hwnd, L"无法恢复系统托盘图标。可在设置窗口暂停映射或退出程序。",
                        kAppName, MB_ICONWARNING | MB_OK);
        }
        return 0;
    }
    switch (msg) {
        case WM_MS_EXEC_MAPPING: {
            // 由钩子投递：在这里执行模拟输入，保证钩子回调足够轻量
            if (!g_config.enabled || wp > 1) return 0;
            const int index = static_cast<int>(wp);
            const Mapping& m = (index == 0) ? g_config.button4 : g_config.button5;
            if (!keymap::Execute(m)) {
                g_config.enabled = false;
                ApplyConfig();
                settings::Show(hwnd);
                MessageBoxW(hwnd,
                    L"快捷键输入未能完整发送，已暂停侧键映射，后续点击将恢复鼠标原始功能。\n"
                    L"高权限窗口、其他输入拦截程序或安全桌面可能限制模拟输入。\n"
                    L"请返回普通应用后再启用映射。",
                    kAppName, MB_ICONWARNING | MB_OK);
            }
            return 0;
        }
        case WM_MS_SHOW_SETTINGS:
            settings::Show(hwnd);
            return 0;
        case WM_MS_TRAY:
            switch (tray::EventMessage(lp)) {
                case WM_LBUTTONDBLCLK:
                    settings::Show(hwnd);
                    break;
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    tray::ShowMenu(hwnd);
                    break;
                default:
                    break;
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case kMenuOpen:
                    settings::Show(hwnd);
                    break;
                case kMenuToggle:
                    g_config.enabled = !g_config.enabled;
                    ApplyConfig();
                    break;
                case kMenuExit:
                    DestroyWindow(hwnd);
                    break;
                default:
                    break;
            }
            return 0;
        case WM_POWERBROADCAST:
            // 睡眠唤醒后确认钩子仍然有效（正常情况下不会被移除）
            if (wp == PBT_APMRESUMEAUTOMATIC && !mousehook::Installed()) {
                mousehook::Install(hwnd);
                ApplyConfig();
            }
            return TRUE;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            QuitApp();
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wp) {
                tray::Remove();
                mousehook::Uninstall();
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool RegisterHiddenClass() {
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HiddenProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = kHiddenClass;
    return RegisterClassExW(&wc) != 0;
}

}  // namespace

AppConfig& GetConfig() {
    return g_config;
}

void ApplyConfig() {
    mousehook::SetEnabled(g_config.enabled);
    mousehook::SetMappings(g_config.button4, g_config.button5);
    tray::Update(g_config.enabled);
    if (!config::Save(g_config)) {
        settings::Show(g_hidden);
        const std::wstring text = L"无法保存配置，当前修改仅在本次运行生效。\n"
            L"请检查文件是否只读、目录权限及磁盘空间：\n" + config::Path();
        MessageBoxW(g_hidden, text.c_str(), kAppName, MB_ICONWARNING | MB_OK);
    }
    settings::RefreshFromConfig();
}

}  // namespace ms

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR cmdLine, int) {
    using namespace ms;

    EnablePerMonitorDpi();

    // 单实例：已运行时唤醒已有实例的设置窗口
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED) {
        if (mutex) CloseHandle(mutex);
        if (HWND existing = FindWindowW(kHiddenClass, nullptr)) {
            PostMessageW(existing, WM_MS_SHOW_SETTINGS, 0, 0);
        }
        return 0;
    }

    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    const bool firstRun = config::Load(g_config);
    const bool startupLaunch = CommandLineHas(cmdLine, L"--startup");

    if (!RegisterHiddenClass() || !settings::Register(hInstance)) {
        MessageBoxW(nullptr, L"界面初始化失败，程序无法启动。", kAppName, MB_ICONERROR | MB_OK);
        return 1;
    }

    g_hidden = CreateWindowExW(0, kHiddenClass, kAppName, 0, 0, 0, 0, 0,
                               nullptr, nullptr, hInstance, nullptr);
    if (!g_hidden) {
        MessageBoxW(nullptr, L"主窗口创建失败，程序无法启动。", kAppName, MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!mousehook::Install(g_hidden)) {
        MessageBoxW(nullptr,
                    L"鼠标监听初始化失败，侧键映射无法工作。\n"
                    L"请确认没有其他安全软件阻止低级鼠标钩子，然后重新启动程序。",
                    kAppName, MB_ICONERROR | MB_OK);
        DestroyWindow(g_hidden);
        return 1;
    }

    mousehook::SetMappings(g_config.button4, g_config.button5);
    mousehook::SetEnabled(g_config.enabled);

    const bool trayAvailable = tray::Add(g_hidden, hInstance);
    if (!trayAvailable) {
        // 托盘不可用不阻断核心功能，但必须让用户知道如何退出
        settings::Show(g_hidden);
        MessageBoxW(nullptr, L"系统托盘图标添加失败，已打开设置窗口。\n"
                             L"可在设置窗口暂停映射或点击“退出程序”。",
                    kAppName, MB_ICONWARNING | MB_OK);
    }
    tray::Update(g_config.enabled);

    if (!startupLaunch || !trayAvailable) settings::Show(g_hidden);
    if (firstRun) ApplyConfig();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    tray::Remove();
    mousehook::Uninstall();
    if (mutex) CloseHandle(mutex);
    return 0;
}
