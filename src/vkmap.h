#pragma once
// vkmap —— 虚拟键码与可读名称的互相转换，用于配置序列化与界面显示。
#include <windows.h>
#include <string>

struct Hotkey {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool win = false;
    UINT vk = 0;  // 0 表示未设置

    bool valid() const { return vk != 0; }
    bool operator==(const Hotkey& o) const {
        return ctrl == o.ctrl && shift == o.shift && alt == o.alt &&
               win == o.win && vk == o.vk;
    }
};

bool VkFromName(const std::wstring& name, UINT* out);
std::wstring VkToName(UINT vk);
std::wstring HotkeyToText(const Hotkey& hk);       // 例：Ctrl+Shift+S
bool HotkeyFromText(const std::wstring& text, Hotkey* out);
bool KeyNeedsExtended(UINT vk);                    // SendInput 需要扩展键标志的按键
