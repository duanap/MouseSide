#include "Model.h"
#include <limits>
namespace ms {
unsigned modifierFor(unsigned key) {
    switch(key) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return Shift;
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return Ctrl;
    case VK_MENU: case VK_LMENU: case VK_RMENU: return Alt;
    case VK_LWIN: case VK_RWIN: return Win;
    default: return 0;
    }
}
bool isModifier(unsigned key) { return modifierFor(key) != 0; }
bool validShortcut(Shortcut s) {
    if ((s.modifiers & ~15u) || s.key < VK_BACK || s.key > 0xFE || isModifier(s.key)) return false;
    if (s.key == VK_PACKET || s.key == VK_PROCESSKEY || s.key == 0xFF || s.key == 0x0A || s.key == 0x0B) return false;
    if (s.key == VK_DELETE && (s.modifiers & (Ctrl | Alt)) == (Ctrl | Alt)) return false;
    if (s.key == 'L' && (s.modifiers & Win)) return false; // secure desktop is not a mapping target
    return MapVirtualKeyW(s.key, MAPVK_VK_TO_VSC) != 0 || (s.key >= VK_BROWSER_BACK && s.key <= VK_LAUNCH_APP2);
}
std::wstring shortcutName(Shortcut s) {
    std::wstring result;
    if (s.modifiers & Ctrl) result += L"Ctrl + ";
    if (s.modifiers & Alt) result += L"Alt + ";
    if (s.modifiers & Shift) result += L"Shift + ";
    if (s.modifiers & Win) result += L"Win + ";
    if (s.key >= 'A' && s.key <= 'Z') return result + static_cast<wchar_t>(s.key);
    if (s.key >= '0' && s.key <= '9') return result + static_cast<wchar_t>(s.key);
    if (s.key >= VK_F1 && s.key <= VK_F24) return result + L"F" + std::to_wstring(s.key - VK_F1 + 1);
    switch (s.key) {
    case VK_TAB: return result + L"Tab";
    case VK_ESCAPE: return result + L"Esc";
    case VK_RETURN: return result + L"Enter";
    case VK_SPACE: return result + L"Space";
    case VK_BROWSER_BACK: return result + L"浏览器后退";
    case VK_BROWSER_FORWARD: return result + L"浏览器前进";
    }
    UINT scan = MapVirtualKeyW(s.key, MAPVK_VK_TO_VSC_EX);
    LONG nameCode = static_cast<LONG>((scan & 0xFFu) << 16);
    if ((scan & 0xFF00u) == 0xE000u) nameCode |= 1 << 24;
    wchar_t buffer[64]{};
    if (GetKeyNameTextW(nameCode, buffer, 64)) result += buffer;
    else result += L"VK " + std::to_wstring(s.key);
    return result;
}
std::wstring actionName(Action a) {
    if (a.kind == ActionKind::Original) return L"保留原始功能";
    if (a.kind == ActionKind::Disabled) return L"禁用此侧键";
    constexpr unsigned keys[] = {'C', 'V', 'X', 'Z', 'Y', 'A'};
    constexpr const wchar_t* names[] = {L"复制", L"粘贴", L"剪切", L"撤销", L"重做", L"全选"};
    for (unsigned i = 0; i < 6; ++i) if (a.shortcut == Shortcut{keys[i], Ctrl}) return names[i];
    if (a.shortcut == Shortcut{VK_BROWSER_BACK, 0}) return L"浏览器后退";
    if (a.shortcut == Shortcut{VK_BROWSER_FORWARD, 0}) return L"浏览器前进";
    return L"自定义快捷键";
}
std::wstring encodeAction(Action a) {
    if (a.kind == ActionKind::Original) return L"original";
    if (a.kind == ActionKind::Disabled) return L"disabled";
    return L"shortcut:" + std::to_wstring(a.shortcut.modifiers) + L":" + std::to_wstring(a.shortcut.key);
}
static std::optional<unsigned> number(std::wstring_view value) {
    if (value.empty() || value.size() > 3) return {};
    unsigned n = 0;
    for (wchar_t c : value) { if (c < L'0' || c > L'9') return {}; n = n * 10 + static_cast<unsigned>(c - L'0'); }
    return n;
}
std::optional<Action> decodeAction(std::wstring_view value) {
    if (value == L"original") return Action{ActionKind::Original, {}};
    if (value == L"disabled") return Action{ActionKind::Disabled, {}};
    if (!value.starts_with(L"shortcut:")) return {};
    value.remove_prefix(9);
    const auto colon = value.find(L':');
    if (colon == std::wstring_view::npos) return {};
    auto mods = number(value.substr(0, colon));
    auto key = number(value.substr(colon + 1));
    if (!mods || !key || !validShortcut({*key, *mods})) return {};
    return Action{ActionKind::Shortcut, {*key, *mods}};
}
unsigned packAction(Action a) { return (static_cast<unsigned>(a.kind) << 16) | (a.shortcut.modifiers << 8) | a.shortcut.key; }
Action unpackAction(unsigned p) { return {static_cast<ActionKind>(p >> 16), {p & 0xFFu, (p >> 8) & 15u}}; }
ButtonDecision ButtonState::onDown(bool enabled, Action a) {
    if (down_) return swallowed_ ? ButtonDecision::Swallow : ButtonDecision::Pass;
    down_ = true;
    swallowed_ = enabled && a.kind != ActionKind::Original;
    if (!swallowed_) return ButtonDecision::Pass;
    return a.kind == ActionKind::Shortcut ? ButtonDecision::Dispatch : ButtonDecision::Swallow;
}
ButtonDecision ButtonState::onUp() {
    const bool swallow = down_ && swallowed_;
    down_ = false;
    swallowed_ = false;
    return swallow ? ButtonDecision::Swallow : ButtonDecision::Pass;
}
}
