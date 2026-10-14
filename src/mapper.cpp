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

// 发送一次完整组合键。修饰键保护策略（Ctrl/Shift/Alt/Win 均按左右侧独立处理）：
//  1. 用户按住但组合不需要的修饰键先逐侧临时松开，发送后恢复；
//  2. 组合需要且两侧都未按住的修饰键注入左侧，结束后释放；
//  3. 主键总是注入一次按下以触发组合；仅当用户未按住该键时才注入松开，
//     避免释放用户正在物理按住的键。
bool SendCombo(const Hotkey& hk) {
    if (!hk.valid()) return false;

    // 修饰键必须逐侧管理：通用 VK（VK_SHIFT/VK_CONTROL/VK_MENU）的注入总是
    // 落在左侧扫描码上，既查询不到右侧状态、也释放不了右侧按键——例如按住
    // 右 Shift 时发送“通用 Shift 松开”仍会留下 Shift 按下状态，导致组合错误。
    struct Side { UINT vk; bool want; bool held; };
    const Side sides[6] = {
        {VK_LCONTROL, hk.ctrl, Held(VK_LCONTROL)},
        {VK_RCONTROL, hk.ctrl, Held(VK_RCONTROL)},
        {VK_LSHIFT,   hk.shift, Held(VK_LSHIFT)},
        {VK_RSHIFT,   hk.shift, Held(VK_RSHIFT)},
        {VK_LMENU,    hk.alt, Held(VK_LMENU)},
        {VK_RMENU,    hk.alt, Held(VK_RMENU)},
    };
    const Side wins[2] = {
        {VK_LWIN, hk.win, Held(VK_LWIN)},
        {VK_RWIN, hk.win, Held(VK_RWIN)},
    };
    const bool keyHeld = Held(hk.vk);
    // 组合需要该类修饰键且两侧都未按住时，由我们注入左侧并在结束后释放
    const bool ctrlInjected = hk.ctrl && !(sides[0].held || sides[1].held);
    const bool shiftInjected = hk.shift && !(sides[2].held || sides[3].held);
    const bool altInjected = hk.alt && !(sides[4].held || sides[5].held);
    const bool winInjected = hk.win && !(wins[0].held || wins[1].held);

    // 记录每个会被注入事件触碰的键的“发送前状态”（最多 9 个：6 个修饰侧 +
    // 2 个 Win + 主键）。部分发送失败时据此把每个键恢复到原状（含用户原本
    // 按住、被我们临时释放的键），而不是仅按注入前缀补松开——否则会破坏
    // 用户的物理按键状态。
    UINT touched[9];
    bool preDown[9];
    int nTouched = 0;
    auto touch = [&](UINT vk) {
        for (int i = 0; i < nTouched; ++i)
            if (touched[i] == vk) return;
        if (nTouched < 9) {
            touched[nTouched] = vk;
            preDown[nTouched] = Held(vk);
            ++nTouched;
        }
    };

    std::vector<INPUT> seq;
    seq.reserve(26);
    auto key = [&](UINT vk, bool up) {
        touch(vk);
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        if (KeyNeedsExtended(vk)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        seq.push_back(in);
    };

    // 1) 释放用户按住但组合不需要的修饰键（逐侧）
    for (const Side& s : sides)
        if (s.held && !s.want) key(s.vk, true);
    for (const Side& s : wins)
        if (s.held && !s.want) key(s.vk, true);
    // 2) 注入组合需要且两侧都未按住的修饰键（左侧）
    if (ctrlInjected) key(VK_LCONTROL, false);
    if (shiftInjected) key(VK_LSHIFT, false);
    if (altInjected) key(VK_LMENU, false);
    if (winInjected) key(VK_LWIN, false);
    // 3) 主键
    key(hk.vk, false);
    if (!keyHeld) key(hk.vk, true);
    // 4) 释放我们注入的修饰键
    if (ctrlInjected) key(VK_LCONTROL, true);
    if (shiftInjected) key(VK_LSHIFT, true);
    if (altInjected) key(VK_LMENU, true);
    if (winInjected) key(VK_LWIN, true);
    // 5) 恢复我们临时释放的修饰键（逐侧）
    for (int i = 5; i >= 0; --i)
        if (sides[i].held && !sides[i].want) key(sides[i].vk, false);
    for (int i = 1; i >= 0; --i)
        if (wins[i].held && !wins[i].want) key(wins[i].vk, false);

    const UINT n = static_cast<UINT>(seq.size());
    const UINT sent = SendInput(n, seq.data(), sizeof(INPUT));
    if (sent == n) return true;

    // SendInput 未全部插入（如桌面切换瞬间）：把每个被触碰的键恢复到
    // 发送前状态——沿注入前缀推演当前状态，与发送前状态比较后补齐差量，
    // 既不留卡键，也不会误松开用户按住的键。向上报告失败。
    MsDbg("sendcombo partial: %u/%u", sent, n);
    bool cur[9];
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
