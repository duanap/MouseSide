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

// 发送一次完整组合键。策略：
//  1. 先松开“用户正按住、但组合不需要”的修饰键，发送后恢复，避免误触组合；
//  2. 组合需要、用户未按住的修饰键由我们按下并在结束后释放；
//  3. 组合需要且用户正按住的修饰键不重复注入，保持用户物理按键状态。
bool SendCombo(const Hotkey& hk) {
    if (!hk.valid()) return false;
    const UINT modVks[4] = {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN};
    const bool want[4] = {hk.ctrl, hk.shift, hk.alt, hk.win};
    bool held[4];
    for (int i = 0; i < 4; ++i)
        held[i] = (GetAsyncKeyState(modVks[i]) & 0x8000) != 0;

    std::vector<INPUT> seq;
    seq.reserve(12);
    auto key = [&seq](UINT vk, bool up) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        if (KeyNeedsExtended(vk)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        seq.push_back(in);
    };

    for (int i = 0; i < 4; ++i)
        if (held[i] && !want[i]) key(modVks[i], true);
    for (int i = 0; i < 4; ++i)
        if (want[i] && !held[i]) key(modVks[i], false);
    key(hk.vk, false);
    key(hk.vk, true);
    for (int i = 3; i >= 0; --i)
        if (want[i] && !held[i]) key(modVks[i], true);
    for (int i = 3; i >= 0; --i)
        if (held[i] && !want[i]) key(modVks[i], false);

    UINT sent = SendInput(static_cast<UINT>(seq.size()), seq.data(), sizeof(INPUT));
    MsDbg("sendcombo size=%zu sent=%u", seq.size(), sent);
    return sent == seq.size();
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
