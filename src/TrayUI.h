// TrayUI.h
//
// 模块 4 / 4：系统托盘与设置窗口
//
//   - 常驻系统托盘（Shell_NotifyIcon），双击打开设置，右键菜单可启用 / 暂停 / 退出
//   - 设置窗口使用 Win32 原生控件自绘卡片，不使用浏览器内核
//   - 关闭设置窗口 = 隐藏到托盘，不等于退出程序
//   - 同时承担「钩子触发动作」的消息分发（钩子回调只投递消息，不直接执行）

#pragma once

#include <windows.h>

#include <string>

#include "HotkeyRecorder.h"

namespace ms {

class App;

class TrayUI {
public:
    static constexpr wchar_t kMessageWindowClass[]  = L"MouseSide.MessageWindow";
    static constexpr wchar_t kSettingsWindowClass[] = L"MouseSide.SettingsWindow";
    static constexpr wchar_t kCardWindowClass[]     = L"MouseSide.Card";

    // 隐藏消息窗口接收的消息
    static constexpr UINT kMsgTriggerAction = WM_APP + 30;  // 鼠标侧键命中映射
    static constexpr UINT kMsgShowSettings  = WM_APP + 31;  // 单实例转发 / 托盘菜单
    static constexpr UINT kMsgTrayNotify    = WM_APP + 32;  // 托盘图标回调

    TrayUI();
    ~TrayUI();

    TrayUI(const TrayUI&)            = delete;
    TrayUI& operator=(const TrayUI&) = delete;

    bool Create(HINSTANCE instance, App* app);
    void Destroy();

    HWND MessageWindow() const noexcept { return m_messageWindow; }

    void ShowSettings();
    void HideSettings();

    // 让设置窗口支持 Tab 键在控件间切换（在消息循环中调用）
    bool PreTranslateMessage(MSG& message);

    // 配置或状态变化后同步界面与托盘
    void RefreshStatus();

    // 打开快捷键录制（拒绝重入）
    void BeginRecording(int buttonIndex);

    void ShowError(const std::wstring& message);

private:
    static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CardWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessageWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleSettingsWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool EnsureWindowClasses();
    bool CreateMessageWindow();
    bool CreateSettingsWindow();
    void DestroyFonts();

    void LayoutSettings();
    void RefreshSettingsContent();
    void RefreshButtonRow(int index);

    void ShowActionMenu(int buttonIndex);
    void ApplyAction(int buttonIndex, const MappedAction& action);

    void AddTrayIcon();
    void RemoveTrayIcon();
    void RefreshTrayIcon();
    void ShowTrayMenu();
    void HandleTrayMessage(LPARAM lParam);

    void OnTriggerAction(int buttonIndex);
    void OnHookWatchdog();

    App*      m_app      = nullptr;
    HINSTANCE m_instance = nullptr;

    HWND m_messageWindow  = nullptr;
    HWND m_settingsWindow = nullptr;

    // 设置窗口控件
    HWND m_titleLabel     = nullptr;
    HWND m_versionLabel   = nullptr;
    HWND m_statusLabel    = nullptr;
    HWND m_card[2]        = {nullptr, nullptr};
    HWND m_buttonTitle[2] = {nullptr, nullptr};
    HWND m_buttonSummary[2] = {nullptr, nullptr};
    HWND m_buttonEdit[2]  = {nullptr, nullptr};
    HWND m_enableCheck    = nullptr;
    HWND m_autoStartCheck = nullptr;
    HWND m_resetButton    = nullptr;

    HFONT  m_titleFont  = nullptr;
    HFONT  m_boldFont   = nullptr;
    HFONT  m_bodyFont   = nullptr;
    HFONT  m_smallFont  = nullptr;
    HBRUSH m_windowBrush = nullptr;
    HBRUSH m_cardBrush   = nullptr;

    HotkeyRecorder m_recorder;

    HICON m_iconRunning = nullptr;
    HICON m_iconPaused  = nullptr;
    bool  m_trayIconAdded = false;
    UINT  m_taskbarCreatedMessage = 0;

    bool m_refreshingControls = false;
};

}  // namespace ms
