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

    // 记录每个会被注入事件触碰的键的“发送前状态”。部分发送失败时据此把
    // 每个键恢复到原状（含用户原本按住、被我们临时释放的键），而不是仅按
    // 注入前缀补松开——否则会破坏用户的物理按键状态。
    UINT touched[8];
    bool preDown[8];
    int nTouched = 0;
    auto touch = [&](UINT vk) {
        for (int i = 0; i < nTouched; ++i)
            if (touched[i] == vk) return;
        if (nTouched < 8) {
            touched[nTouched] = vk;
            preDown[nTouched] = Held(vk);
            ++nTouched;
        }
    };

    std::vector<INPUT> seq;
    seq.reserve(14);
    auto key = [&](UINT vk, bool up) {
        touch(vk);
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

    // SendInput 未全部插入（如桌面切换瞬间）：把每个被触碰的键恢复到
    // 发送前状态——沿注入前缀推演当前状态，与发送前状态比较后补齐差量，
    // 既不留卡键，也不会误松开用户按住的键。向上报告失败。
    MsDbg("sendcombo partial: %u/%u", sent, n);
    bool cur[8];
    for (int t = 0; t < nTouched; ++t) cur[t] = preDown[t];
    for (UINT i = 0; i < sent && i < n; ++i) {
        const UINT vk = seq[i].ki.wVk;
        const bool up = (seq[i].ki.dwFlags & KEYEVENTF_KEYUP) != 0;
        for (int t = 0; t < nTouched; ++t)
            if (touched[t] == vk) { cur[t] = !up; break; }
    }
    std::vector<INPUT> fix;
    for (int t = 0; t < nTouched; ++t) {
        if (cur[t] == preDown[t]) continue;
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(touched[t]);
        in.ki.dwFlags = preDown[t] ? 0 : KEYEVENTF_KEYUP;
        if (KeyNeedsExtended(touched[t])) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        fix.push_back(in);
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
            // 无操作（钩子层已放行/吞掉），视为成功，
            // 避免误触发“发送失败”的用户提示。
            return true;
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
