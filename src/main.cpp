// main.cpp
//
// 程序入口：
//   1. 单实例检查（已有实例则唤醒其设置窗口后退出）
//   2. 初始化通用控件
//   3. 交给 App 完成初始化并进入消息循环

#include <windows.h>

#include <commctrl.h>

#include <iterator>
#include <string>

#include "App.h"
#include "AppInfo.h"

namespace {

// 单实例互斥体。句柄在进程生命周期内保持有效，无需显式释放。
HANDLE g_singleInstanceMutex = nullptr;

constexpr wchar_t kMutexName[] = L"Local\\MouseSide.SingleInstance";

// 返回 false 表示已有实例在运行
bool AcquireSingleInstance() {
    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (g_singleInstanceMutex == nullptr) {
        // 极端情况下不阻塞启动，交由后续逻辑自行处理
        return true;
    }
    return GetLastError() != ERROR_ALREADY_EXISTS;
}

// 已有实例在运行时，唤醒它的设置窗口
void WakeExistingInstance() {
    for (int attempt = 0; attempt < 30; ++attempt) {
        HWND window = FindWindowW(ms::TrayUI::kMessageWindowClass, nullptr);
        if (window != nullptr) {
            PostMessageW(window, ms::TrayUI::kMsgShowSettings, 0, 0);
            return;
        }
        Sleep(100);  // 另一个实例可能仍在启动过程中
    }
}

// 开机自启由 Run 项以 "--tray" 参数拉起，此时只进托盘
bool CommandLineWantsTrayOnly() {
    const std::wstring commandLine = GetCommandLineW();
    return commandLine.find(L"--tray") != std::wstring::npos;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    if (!AcquireSingleInstance()) {
        WakeExistingInstance();
        return 0;
    }

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = static_cast<DWORD>(sizeof(commonControls));
    commonControls.dwICC  = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    ms::App& app = ms::App::Get();
    if (!app.Initialize(instance, CommandLineWantsTrayOnly())) {
        return 1;
    }

    return app.Run();
}
