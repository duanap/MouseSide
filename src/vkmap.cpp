#include "vkmap.h"

#include <cstdio>
#include <cwctype>
#include <vector>

namespace {

struct VkEntry {
    UINT vk;
    const wchar_t* name;
};

// 字母、数字、F1-F24 由代码生成；这里只放具名按键
const VkEntry kNamedVks[] = {
    {VK_ESCAPE, L"Esc"},       {VK_TAB, L"Tab"},        {VK_SPACE, L"Space"},
    {VK_RETURN, L"Enter"},     {VK_BACK, L"Backspace"},
    {VK_PRIOR, L"PgUp"},       {VK_NEXT, L"PgDn"},      {VK_END, L"End"},
    {VK_HOME, L"Home"},
    {VK_LEFT, L"Left"},        {VK_UP, L"Up"},          {VK_RIGHT, L"Right"},
    {VK_DOWN, L"Down"},
    {VK_INSERT, L"Ins"},       {VK_DELETE, L"Del"},     {VK_SNAPSHOT, L"PrtSc"},
    {VK_SCROLL, L"ScrollLock"},{VK_PAUSE, L"Pause"},    {VK_APPS, L"Menu"},
    {VK_NUMPAD0, L"Num0"},     {VK_NUMPAD1, L"Num1"},   {VK_NUMPAD2, L"Num2"},
    {VK_NUMPAD3, L"Num3"},     {VK_NUMPAD4, L"Num4"},   {VK_NUMPAD5, L"Num5"},
    {VK_NUMPAD6, L"Num6"},     {VK_NUMPAD7, L"Num7"},   {VK_NUMPAD8, L"Num8"},
    {VK_NUMPAD9, L"Num9"},
    {VK_ADD, L"Num+"},         {VK_SUBTRACT, L"Num-"},  {VK_MULTIPLY, L"Num*"},
    {VK_DIVIDE, L"Num/"},      {VK_DECIMAL, L"Num."},   {VK_NUMLOCK, L"NumLock"},
    {VK_BROWSER_BACK, L"BrowserBack"},       {VK_BROWSER_FORWARD, L"BrowserForward"},
    {VK_BROWSER_REFRESH, L"BrowserRefresh"}, {VK_BROWSER_STOP, L"BrowserStop"},
    {VK_BROWSER_SEARCH, L"BrowserSearch"},   {VK_BROWSER_FAVORITES, L"BrowserFavorites"},
    {VK_BROWSER_HOME, L"BrowserHome"},
    {VK_MEDIA_PREV_TRACK, L"MediaPrev"},     {VK_MEDIA_PLAY_PAUSE, L"MediaPlay"},
    {VK_MEDIA_NEXT_TRACK, L"MediaNext"},     {VK_MEDIA_STOP, L"MediaStop"},
    {VK_VOLUME_MUTE, L"VolMute"},            {VK_VOLUME_DOWN, L"VolDown"},
    {VK_VOLUME_UP, L"VolUp"},
};

std::wstring Lower(const std::wstring& s) {
    std::wstring r = s;
    for (wchar_t& c : r) c = static_cast<wchar_t>(towlower(c));
    return r;
}

}  // namespace

bool VkFromName(const std::wstring& name, UINT* out) {
    if (name.empty()) return false;
    // 单字符：字母 A-Z、数字 0-9
    if (name.size() == 1) {
        wchar_t c = towupper(name[0]);
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
            *out = static_cast<UINT>(c);
            return true;
        }
    }
    // F1 - F24
    const std::wstring low = Lower(name);
    if (low.size() >= 2 && low[0] == L'f') {
        bool digits = true;
        for (size_t i = 1; i < low.size(); ++i)
            if (low[i] < L'0' || low[i] > L'9') { digits = false; break; }
        if (digits) {
            const int n = static_cast<int>(wcstol(low.c_str() + 1, nullptr, 10));
            if (n >= 1 && n <= 24) {
                *out = VK_F1 + static_cast<UINT>(n) - 1;
                return true;
            }
        }
    }
    for (const VkEntry& e : kNamedVks) {
        if (low == Lower(e.name)) {
            *out = e.vk;
            return true;
        }
    }
    return false;
}

std::wstring VkToName(UINT vk) {
    if (vk >= L'A' && vk <= L'Z') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= L'0' && vk <= L'9') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    for (const VkEntry& e : kNamedVks)
        if (e.vk == vk) return e.name;
    wchar_t buf[16];
    swprintf(buf, 16, L"VK 0x%02X", vk);
    return buf;
}

std::wstring HotkeyToText(const Hotkey& hk) {
    if (!hk.valid()) return L"";
    std::wstring s;
    if (hk.ctrl) s += L"Ctrl+";
    if (hk.shift) s += L"Shift+";
    if (hk.alt) s += L"Alt+";
    if (hk.win) s += L"Win+";
    s += VkToName(hk.vk);
    return s;
}

bool HotkeyFromText(const std::wstring& text, Hotkey* out) {
    *out = Hotkey{};
    if (text.empty()) return false;
    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (true) {
        size_t pos = text.find(L'+', start);
        if (pos == std::wstring::npos) {
            tokens.push_back(text.substr(start));
            break;
        }
        tokens.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    if (tokens.back().empty() && tokens.size() > 1) tokens.pop_back();  // 容忍尾部分隔符
    if (tokens.empty()) return false;

    Hotkey hk;
    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        const std::wstring t = Lower(tokens[i]);
        bool* flag = nullptr;
        if (t == L"ctrl" || t == L"control") flag = &hk.ctrl;
        else if (t == L"shift") flag = &hk.shift;
        else if (t == L"alt") flag = &hk.alt;
        else if (t == L"win" || t == L"windows") flag = &hk.win;
        if (!flag || *flag) return false;  // 未知或重复的修饰键
        *flag = true;
    }
    if (!VkFromName(tokens.back(), &hk.vk)) return false;
    *out = hk;
    return true;
}

bool KeyNeedsExtended(UINT vk) {
    switch (vk) {
        case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_INSERT: case VK_DELETE: case VK_SNAPSHOT: case VK_DIVIDE:
            return true;
        default:
            return false;
    }
}
