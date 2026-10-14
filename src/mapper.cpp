#include "mapper.h"

#include <vector>

#include "dbglog.h"

namespace {

struct PresetDef {
    PresetId id;
    const wchar_t* name;
    Hotkey hk;
};

Hotkey HK(bool c, bool s, bool a, bool w, UINT vk) {
    Hotkey hk;
    hk.ctrl = c; hk.shift = s; hk.alt = a; hk.win = w; hk.vk = vk;
    return hk;
}

const PresetDef kPresets[] = {
    {PresetId::Copy,          L"复制",     HK(true,  false, false, false, L'C')},
    {PresetId::Paste,         L"粘贴",     HK(true,  false, false, false, L'V')},
    {PresetId::Cut,           L"剪切",     HK(true,  false, false, false, L'X')},
    {PresetId::Undo,          L"撤销",     HK(true,  false, false, false, L'Z')},
    {PresetId::Redo,          L"重做",     HK(true,  false, false, false, L'Y')},
    {PresetId::SelectAll,     L"全选",     HK(true,  false, false, false, L'A')},
    {PresetId::BrowserBack,   L"浏览器后退", HK(false, false, false, false, VK_BROWSER_BACK)},
    {PresetId::BrowserForward, L"浏览器前进", HK(false, false, false, false, VK_BROWSER_FORWARD)},
};

const PresetDef* FindPreset(PresetId id) {
    for (const PresetDef& p : kPresets)
        if (p.id == id) return &p;
    return &kPresets[0];
}

bool Held(UINT vk) { return (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0; }

// 发送一次完整组合键。修饰键保护策略：
//  1. 用户按住但组合不需要的修饰键先临时松开，发送后恢复（Win 左右键都要处理）；
//  2. 组合需要、用户未按住的修饰键由我们按下并在结束后释放；
//  3. 主键总是注入一次按下以触发组合；仅当用户未按住该键时才注入松开，
//     避免释放用户正在物理按住的键。
bool SendCombo(const Hotkey& hk) {
    if (!hk.valid()) return false;
    struct Plain { UINT vk; bool want; bool held; };
    const Plain mods[3] = {
        {VK_CONTROL, hk.ctrl, Held(VK_CONTROL)},
        {VK_SHIFT, hk.shift, Held(VK_SHIFT)},
        {VK_MENU, hk.alt, Held(VK_MENU)},
    };
    const bool lwinHeld = Held(VK_LWIN);
    const bool rwinHeld = Held(VK_RWIN);
    const bool winHeld = lwinHeld || rwinHeld;
    const bool keyHeld = Held(hk.vk);

    std::vector<INPUT> seq;
    seq.reserve(14);
    auto key = [&seq](UINT vk, bool up) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        if (KeyNeedsExtended(vk)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        seq.push_back(in);
    };

    // 1) 释放用户按住但组合不需要的修饰键
    for (const Plain& m : mods)
        if (m.held && !m.want) key(m.vk, true);
    if (winHeld && !hk.win) {
        if (lwinHeld) key(VK_LWIN, true);
        if (rwinHeld) key(VK_RWIN, true);
    }
    // 2) 注入组合需要但用户未按住的修饰键
    for (const Plain& m : mods)
        if (m.want && !m.held) key(m.vk, false);
    if (hk.win && !winHeld) key(VK_LWIN, false);
    // 3) 主键
    key(hk.vk, false);
    if (!keyHeld) key(hk.vk, true);
    // 4) 释放我们注入的修饰键
    if (hk.win && !winHeld) key(VK_LWIN, true);
    for (int i = 2; i >= 0; --i)
        if (mods[i].want && !mods[i].held) key(mods[i].vk, true);
    // 5) 恢复我们临时释放的冲突修饰键
    if (winHeld && !hk.win) {
        if (rwinHeld) key(VK_RWIN, false);
        if (lwinHeld) key(VK_LWIN, false);
    }
    for (int i = 2; i >= 0; --i)
        if (mods[i].held && !mods[i].want) key(mods[i].vk, false);

    const UINT n = static_cast<UINT>(seq.size());
    const UINT sent = SendInput(n, seq.data(), sizeof(INPUT));
    if (sent == n) return true;

    // SendInput 未全部插入（如桌面切换瞬间）：为已注入的按下补发松开，
    // 避免留下“卡键”，并向上报告失败。
    MsDbg("sendcombo partial: %u/%u", sent, n);
    int state[256] = {};
    for (UINT i = 0; i < sent; ++i) {
        if (seq[i].type != INPUT_KEYBOARD) continue;
        const UINT vk = seq[i].ki.wVk;
        if (!(seq[i].ki.dwFlags & KEYEVENTF_KEYUP)) {
            ++state[vk];
        } else if (state[vk] > 0) {
            --state[vk];
        }
    }
    std::vector<INPUT> fix;
    for (UINT vk = 0; vk < 256; ++vk) {
        if (state[vk] > 0) {
            INPUT up{};
            up.type = INPUT_KEYBOARD;
            up.ki.wVk = static_cast<WORD>(vk);
            up.ki.dwFlags = KEYEVENTF_KEYUP;
            if (KeyNeedsExtended(vk)) up.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            fix.push_back(up);
        }
    }
    if (!fix.empty())
        SendInput(static_cast<UINT>(fix.size()), fix.data(), sizeof(INPUT));
    return false;
}

}  // namespace

namespace mapper {

bool Execute(const ButtonConfig& bc) {
    switch (bc.type) {
        case ActionType::Preset:
            return SendCombo(PresetHotkey(bc.preset));
        case ActionType::Keys:
            return SendCombo(bc.hotkey);
        case ActionType::Passthrough:
        case ActionType::Disabled:
            return false;  // 不拦截，钩子层已放行/吞掉
    }
    return false;
}

std::wstring PresetDisplayName(PresetId id) {
    const PresetDef* p = FindPreset(id);
    if (id == PresetId::BrowserBack || id == PresetId::BrowserForward)
        return p->name;
    return std::wstring(p->name) + L" (" + HotkeyToText(p->hk) + L")";
}

Hotkey PresetHotkey(PresetId id) {
    return FindPreset(id)->hk;
}

void EachPreset(void (*fn)(PresetId id, const std::wstring& name, void* ctx), void* ctx) {
    for (const PresetDef& p : kPresets)
        fn(p.id, PresetDisplayName(p.id), ctx);
}

}  // namespace mapper
