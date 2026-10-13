// MouseSide - 映射执行与显示文本（SendInput 模拟键盘输入）
#pragma once

#include "Common.h"

namespace ms {
namespace keymap {

// 预设功能的中文显示名
const wchar_t* PresetDisplayName(PresetAction a);
// 预设功能的完整显示名，例如 "粘贴 (Ctrl + V)"
std::wstring PresetDisplayFull(PresetAction a);

// 单个按键名，例如 "S"、"F5"、"Esc"
std::wstring KeyName(unsigned vk);
// 组合键文本，例如 "Ctrl + Shift + S"
std::wstring HotkeyText(const Hotkey& hk);
// 映射的可读文本，例如 "粘贴：Ctrl + V"
std::wstring MappingText(const Mapping& m);

// 执行映射（不处理 Original / Disabled）
void Execute(const Mapping& m);

}  // namespace keymap
}  // namespace ms
