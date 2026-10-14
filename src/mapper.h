#pragma once
// mapper —— KeyMapper：把侧键配置翻译成一次 SendInput 键盘序列。
#include "config.h"

namespace mapper {

// 执行一次映射（在主线程、宿主窗口收到 WM_APP_EXEC 时调用）
bool Execute(const ButtonConfig& bc);

// 预设显示名，例：L"复制 (Ctrl+C)"
std::wstring PresetDisplayName(PresetId id);
// 预设对应的快捷键（浏览器后退/前进使用 VK_BROWSER_*）
Hotkey PresetHotkey(PresetId id);
// 按顺序返回全部预设（供下拉框使用）
void EachPreset(void (*fn)(PresetId id, const std::wstring& name, void* ctx), void* ctx);

}  // namespace mapper
