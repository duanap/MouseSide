// MouseSide - 公共类型与常量定义
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>

namespace ms {

// ---- 常量 ----
constexpr wchar_t kAppName[]       = L"MouseSide";
constexpr wchar_t kVersionText[]   = L"v1.0.2";
constexpr wchar_t kMutexName[]     = L"MouseSide_SingleInstance_5A1DC4";
constexpr wchar_t kHiddenClass[]   = L"MouseSide_HiddenWindow";
constexpr wchar_t kSettingsClass[] = L"MouseSide_SettingsWindow";
constexpr wchar_t kCaptureClass[]  = L"MouseSide_CaptureWindow";
constexpr wchar_t kRunValueName[]  = L"MouseSide";

// ---- 自定义窗口消息 ----
// 隐藏窗口收到后执行对应侧键的映射动作。wParam: 0 = XBUTTON1, 1 = XBUTTON2
constexpr UINT WM_MS_EXEC_MAPPING   = WM_APP + 1;
// 唤醒已有实例：打开设置窗口
constexpr UINT WM_MS_SHOW_SETTINGS  = WM_APP + 2;
// 托盘图标回调消息（NOTIFYICONDATA.uCallbackMessage）
constexpr UINT WM_MS_TRAY           = WM_APP + 3;
// 隐藏窗口 -> 设置窗口：配置在其他地方（托盘菜单）被修改，刷新 UI
constexpr UINT WM_MS_STATE_CHANGED  = WM_APP + 4;
// 捕获窗口 -> 设置窗口：同步发送录制结果。wParam: 1=成功, 0=取消；lParam: 结果快照
constexpr UINT WM_MS_CAPTURE_RESULT = WM_APP + 5;

// ---- 托盘菜单命令 ID ----
constexpr int kMenuOpen   = 1001;
constexpr int kMenuToggle = 1002;
constexpr int kMenuExit   = 1003;

// ---- 映射动作 ----
enum class ActionKind {
    Preset,    // 预设功能（复制、粘贴等）
    Custom,    // 自定义键盘快捷键
    Original,  // 保留鼠标原始功能（不拦截）
    Disabled,  // 禁用此侧键（拦截但不执行任何动作）
};

enum class PresetAction {
    Copy, Paste, Cut, Undo, Redo, SelectAll,
    BrowserBack, BrowserForward,
};

struct Hotkey {
    unsigned vk = 0;  // 主键虚拟键码，0 表示空
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool win = false;
    bool IsEmpty() const { return vk == 0; }
};

constexpr bool IsModifierKey(unsigned vk) {
    switch (vk) {
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU: case VK_LMENU: case VK_RMENU:
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        case VK_LWIN: case VK_RWIN: return true;
        default: return false;
    }
}

constexpr bool IsShortcutMainKey(unsigned vk) {
    return vk > 0 && vk < 255 && !IsModifierKey(vk) &&
           vk != VK_LBUTTON && vk != VK_RBUTTON && vk != VK_MBUTTON &&
           vk != VK_XBUTTON1 && vk != VK_XBUTTON2;
}

struct Mapping {
    ActionKind kind = ActionKind::Preset;
    PresetAction preset = PresetAction::Paste;
    Hotkey hotkey{};
};

struct AppConfig {
    bool enabled = true;
    Mapping button4;  // XBUTTON1，鼠标第四按钮
    Mapping button5;  // XBUTTON2，鼠标第五按钮
    bool autoStart = false;
};

// ---- 预设功能表 ----
struct PresetDef {
    PresetAction id;
    const wchar_t* label;
    Hotkey key;
};

const PresetDef* PresetTable();       // 结尾以 label == nullptr 结束
Hotkey PresetHotkey(PresetAction a);
const wchar_t* PresetLabel(PresetAction a);

// ---- 全局配置（main.cpp 实现）----
AppConfig& GetConfig();
// 将当前配置应用到鼠标钩子与托盘图标，并写入配置文件
void ApplyConfig();

}  // namespace ms
