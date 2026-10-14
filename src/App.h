// App.h
//
// 应用主控：串联配置、鼠标钩子与托盘界面，并承载消息循环。

#pragma once

#include <windows.h>

#include <string>

#include "Config.h"
#include "KeyMapper.h"
#include "MouseHook.h"
#include "TrayUI.h"

namespace ms {

class App {
public:
    static App& Get();

    // startHidden = true 时（开机自启）只进托盘，不弹出设置窗口
    bool Initialize(HINSTANCE instance, bool startHidden);

    // 消息循环，返回进程退出码。内部在退出时完成资源释放。
    int Run();

    void RequestExit();

    // --- 配置访问 -----------------------------------------------------------
    const AppConfig& GetConfig() const noexcept { return m_config; }
    MappedAction GetButtonAction(int index) const;
    bool IsEnabled() const noexcept { return m_config.enabled; }
    bool IsAutoStartEnabled() const;
    const std::wstring& ConfigPath() const noexcept { return m_configPath; }

    // --- 配置修改（立即生效并落盘）------------------------------------------
    void SetEnabled(bool enabled);
    void ToggleEnabled();
    void SetButtonAction(int index, const MappedAction& action);
    void SetAutoStart(bool enabled);
    void ResetToDefaults();

    // --- 运行时 -------------------------------------------------------------
    // 执行某个侧键的映射动作（由 UI 线程的消息处理调用）
    void TriggerAction(int index);

    // 低频钩子自愈：仅由界面层的定时器调用
    void RefreshHook();

private:
    App() = default;
    App(const App&)            = delete;
    App& operator=(const App&) = delete;

    void Shutdown();
    void ApplyHookInterception();
    void Persist();

    AppConfig    m_config;
    MouseHook    m_hook;
    TrayUI       m_ui;
    std::wstring m_configPath;
    HINSTANCE    m_instance = nullptr;
};

}  // namespace ms
