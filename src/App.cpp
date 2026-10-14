// App.cpp

#include "App.h"

#include <iterator>
#include <string>

#include "AppInfo.h"

namespace ms {
namespace {

// 开机自启使用「当前用户级」启动项，无需管理员权限
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"MouseSide";

// 自启动时附加该参数，用于「直接进托盘、不弹设置窗口」
constexpr wchar_t kTrayArgument[] = L"--tray";

}  // namespace

App& App::Get() {
    static App instance;
    return instance;
}

// ---------------------------------------------------------------------------
// 启动 / 退出
// ---------------------------------------------------------------------------

bool App::Initialize(HINSTANCE instance, bool startHidden) {
    m_instance   = instance;
    m_configPath = Config::DefaultPath();
    m_config     = Config::Load(m_configPath);

    if (!m_ui.Create(instance, this)) {
        MessageBoxW(nullptr,
                    L"MouseSide 启动失败：无法创建窗口或注册系统托盘。",
                    kAppName,
                    MB_OK | MB_ICONERROR);
        return false;
    }

    // 钩子回调只投递消息，真正的注入在消息循环里完成
    m_hook.SetTriggerCallback([](int buttonIndex) {
        HWND target = App::Get().m_ui.MessageWindow();
        if (target != nullptr) {
            PostMessageW(target,
                         TrayUI::kMsgTriggerAction,
                         static_cast<WPARAM>(buttonIndex),
                         0);
        }
    });

    ApplyHookInterception();

    if (!m_hook.Install()) {
        std::wstring text = L"鼠标监听初始化失败（错误码 ";
        text += std::to_wstring(m_hook.LastError());
        text += L"）。\n\n侧键映射当前不可用，鼠标原始功能不受影响。\n"
                L"程序可继续在托盘运行，稍后会自动重试初始化。";
        m_ui.ShowError(text);
    }

    m_ui.RefreshStatus();

    if (!startHidden) {
        m_ui.ShowSettings();
    }

    return true;
}

int App::Run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (m_ui.PreTranslateMessage(message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Shutdown();
    return 0;
}

void App::Shutdown() {
    // 先摘钩子，保证退出后侧键立刻恢复 Windows 原始行为
    m_hook.Uninstall();

    Persist();
    m_ui.Destroy();
    m_instance = nullptr;
}

void App::RequestExit() {
    m_ui.HideSettings();
    PostQuitMessage(0);
}

// ---------------------------------------------------------------------------
// 配置访问与修改
// ---------------------------------------------------------------------------

MappedAction App::GetButtonAction(int index) const {
    if (index < 0 || index >= kButtonCount) {
        return MappedAction{};
    }
    return m_config.buttons[index];
}

void App::ApplyHookInterception() {
    // 暂停时保留钩子但关闭拦截，恢复时可立即生效
    m_hook.SetEnabled(m_config.enabled);

    for (int i = 0; i < kButtonCount; ++i) {
        const ActionType type = m_config.buttons[i].type;
        // 「保留原始功能」和「禁用」都不需要吞事件
        const bool intercept =
            (type != ActionType::KeepOriginal) && (type != ActionType::Disabled);
        m_hook.SetIntercept(i, intercept);
    }
}

void App::Persist() {
    if (m_configPath.empty()) {
        return;
    }
    if (!Config::Save(m_configPath, m_config)) {
        // 不打断用户，仅留下调试信息
        OutputDebugStringW(L"[MouseSide] 配置写入失败\n");
    }
}

void App::SetEnabled(bool enabled) {
    m_config.enabled = enabled;
    ApplyHookInterception();
    m_ui.RefreshStatus();
    Persist();
}

void App::ToggleEnabled() {
    SetEnabled(!m_config.enabled);
}

void App::SetButtonAction(int index, const MappedAction& action) {
    if (index < 0 || index >= kButtonCount) {
        return;
    }

    m_config.buttons[index] = action;
    Config::Sanitize(m_config);

    ApplyHookInterception();
    m_ui.RefreshStatus();
    Persist();
}

void App::ResetToDefaults() {
    // 「开机自动启动」属于系统层集成，不随映射配置一起重置
    const bool keepAutoStart = m_config.autoStart;

    m_config           = Config::MakeDefault();
    m_config.autoStart = keepAutoStart;

    ApplyHookInterception();
    m_ui.RefreshStatus();
    Persist();
}

// ---------------------------------------------------------------------------
// 动作执行
// ---------------------------------------------------------------------------

void App::TriggerAction(int index) {
    if (!m_config.enabled || index < 0 || index >= kButtonCount) {
        return;
    }

    const MappedAction& action = m_config.buttons[index];
    if (action.type == ActionType::KeepOriginal ||
        action.type == ActionType::Disabled) {
        return;
    }

    if (!ExecuteAction(action)) {
        OutputDebugStringW(L"[MouseSide] 映射动作执行失败\n");
    }
}

void App::RefreshHook() {
    if (m_hook.IsInstalled()) {
        m_hook.Refresh();
        return;
    }

    // 启动时安装失败，这里自动重试一次
    ApplyHookInterception();
    m_hook.Install();
}

// ---------------------------------------------------------------------------
// 开机自启（HKCU Run，用户级）
// ---------------------------------------------------------------------------

bool App::IsAutoStartEnabled() const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key)
        != ERROR_SUCCESS) {
        return false;
    }

    wchar_t buffer[512] = {};
    DWORD   size = static_cast<DWORD>(sizeof(buffer));
    DWORD   type = 0;

    const LSTATUS status = RegQueryValueExW(key,
                                            kRunValueName,
                                            nullptr,
                                            &type,
                                            reinterpret_cast<LPBYTE>(buffer),
                                            &size);
    RegCloseKey(key);

    return status == ERROR_SUCCESS && type == REG_SZ && buffer[0] != L'\0';
}

void App::SetAutoStart(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr)
        != ERROR_SUCCESS) {
        m_ui.ShowError(L"无法访问开机启动项，请在当前用户下重试。");
        return;
    }

    bool ok = false;
    if (enabled) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(std::size(exePath)));

        std::wstring command = L"\"";
        command += exePath;
        command += L"\" ";
        command += kTrayArgument;

        const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        ok = RegSetValueExW(key,
                            kRunValueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            bytes) == ERROR_SUCCESS;
    } else {
        const LSTATUS status = RegDeleteValueW(key, kRunValueName);
        ok = (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(key);

    if (!ok) {
        m_ui.ShowError(enabled ? L"设置开机自动启动失败。"
                               : L"取消开机自动启动失败。");
    }

    // 以注册表中的实际状态为准回写到配置
    m_config.autoStart = IsAutoStartEnabled();
    m_ui.RefreshStatus();
    Persist();
}

}  // namespace ms
