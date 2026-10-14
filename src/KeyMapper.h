// KeyMapper.h
//
// 模块 2 / 4：按键映射与执行
//
//   - 定义映射动作的数据模型（ActionType / MappedAction）
//   - 负责把动作翻译成实际的输入事件（SendInput）
//   - 负责动作与字符串之间的序列化（供 Config 使用）以及界面显示文案

#pragma once

#include <windows.h>

#include <string>

namespace ms {

// ---------------------------------------------------------------------------
// 动作类型
// ---------------------------------------------------------------------------

enum class ActionType {
    Paste,         // 粘贴        Ctrl + V
    Copy,          // 复制        Ctrl + C
    Cut,           // 剪切        Ctrl + X
    Undo,          // 撤销        Ctrl + Z
    Redo,          // 重做        Ctrl + Y
    SelectAll,     // 全选        Ctrl + A
    Back,          // 浏览器后退（重放 XBUTTON1 事件）
    Forward,       // 浏览器前进（重放 XBUTTON2 事件）
    Custom,        // 用户录制的自定义快捷键
    KeepOriginal,  // 保留鼠标原始功能（放行原始事件）
    Disabled       // 禁用此侧键（吞掉事件且不执行任何操作）
};

// ---------------------------------------------------------------------------
// 修饰键位掩码
// ---------------------------------------------------------------------------

enum ModifierBits : UINT {
    MOD_NONE  = 0x0,
    MOD_CTRL  = 0x1,
    MOD_ALT   = 0x2,
    MOD_SHIFT = 0x4,
    MOD_WIN   = 0x8
};

// ---------------------------------------------------------------------------
// 动作描述
// ---------------------------------------------------------------------------

struct MappedAction {
    ActionType type = ActionType::KeepOriginal;
    UINT vk   = 0;  // 仅 ActionType::Custom 使用
    UINT mods = 0;  // 仅 ActionType::Custom 使用
};

// 注入事件标记（'MSID'）。
// 用于让本程序自己的鼠标钩子识别并放行自身注入的事件，避免递归拦截。
// 同时保证「浏览器后退 / 前进」这类需要重放鼠标事件的动作不会被自己吃掉。
inline constexpr ULONG_PTR kInjectedMarker = static_cast<ULONG_PTR>(0x4D534944u);

// ---------------------------------------------------------------------------
// 动作元数据
// ---------------------------------------------------------------------------

// INI 中使用的稳定标识（ASCII），与界面语言解耦
const wchar_t* ActionTypeName(ActionType type) noexcept;
ActionType ActionTypeFromName(const std::wstring& name, ActionType fallback) noexcept;

// 界面显示名（简体中文）
const wchar_t* ActionTypeLabel(ActionType type) noexcept;

// 是否为「预设快捷键」动作，是则回填 mods / vk
bool ActionPresetKeys(ActionType type, UINT& mods, UINT& vk) noexcept;

// 界面第二行文案，例如「Ctrl + V」「直接放行，不拦截」
std::wstring ActionSummary(const MappedAction& action);

// 快捷键组合的显示文本，例如「Ctrl + Shift + S」
std::wstring HotkeyToString(UINT mods, UINT vk);

// 单个虚拟键的显示名，例如「V」「F5」「←」
std::wstring VirtualKeyName(UINT vk);

// ---------------------------------------------------------------------------
// 执行
// ---------------------------------------------------------------------------

// 执行一次映射动作。返回 false 表示未能注入输入（如被 UIPI 拦截）。
bool ExecuteAction(const MappedAction& action);

// 发送一次键盘组合（修饰键按下 -> 按键按下/抬起 -> 修饰键抬起，单批完成）
void SendKeyStroke(UINT vk, UINT mods);

// 重放一次鼠标扩展按钮事件（forward = true 表示前进 / XBUTTON2）
void SendMouseXButton(bool forward);

// 判断某虚拟键是否为修饰键
bool IsModifierKey(UINT vk) noexcept;

// 该虚拟键在 SendInput 中是否需要 KEYEVENTF_EXTENDEDKEY
bool IsExtendedKey(UINT vk) noexcept;

}  // namespace ms
