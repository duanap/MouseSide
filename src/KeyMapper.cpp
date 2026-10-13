#include "KeyMapper.h"

#include <string>
#include <vector>

namespace ms {
namespace keymap {

namespace {

bool PhysicallyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void AppendKey(std::vector<INPUT>& inputs, WORD vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    if (up) in.ki.dwFlags |= KEYEVENTF_KEYUP;
    inputs.push_back(in);
}

// 发送组合键：修饰键 down -> 主键 down/up -> 修饰键 up
// 若物理键盘上按下了本次组合不需要的修饰键，先释放，发送完成后（仍处于按下状态时）补回，
// 避免物理修饰键污染目标组合。
void SendHotkey(const Hotkey& hk) {
    if (hk.IsEmpty()) return;

    const int modifiers[4][2] = {
        {VK_CONTROL, static_cast<int>(hk.ctrl)},
        {VK_MENU,    static_cast<int>(hk.alt)},
        {VK_SHIFT,   static_cast<int>(hk.shift)},
        {VK_LWIN,    static_cast<int>(hk.win)},
    };

    int released[4] = {0, 0, 0, 0};
    std::vector<INPUT> inputs;
    inputs.reserve(12);

    for (int i = 0; i < 4; ++i) {
        const int vk = modifiers[i][0];
        if (!modifiers[i][1] && PhysicallyDown(vk)) {
            AppendKey(inputs, static_cast<WORD>(vk), true);
            released[i] = 1;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (modifiers[i][1]) AppendKey(inputs, static_cast<WORD>(modifiers[i][0]), false);
    }

    AppendKey(inputs, static_cast<WORD>(hk.vk), false);
    AppendKey(inputs, static_cast<WORD>(hk.vk), true);

    for (int i = 3; i >= 0; --i) {
        if (modifiers[i][1]) AppendKey(inputs, static_cast<WORD>(modifiers[i][0]), true);
    }
    for (int i = 0; i < 4; ++i) {
        // 仅当用户仍按住时才补回，避免留下"卡住"的修饰键
        if (released[i] && PhysicallyDown(modifiers[i][0])) {
            AppendKey(inputs, static_cast<WORD>(modifiers[i][0]), false);
        }
    }

    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
}

const wchar_t* FallbackKeyName(unsigned vk) {
    switch (vk) {
        case VK_BROWSER_BACK:      return L"后退";
        case VK_BROWSER_FORWARD:   return L"前进";
        case VK_BROWSER_REFRESH:   return L"刷新";
        case VK_BROWSER_STOP:      return L"停止";
        case VK_BROWSER_SEARCH:    return L"搜索";
        case VK_BROWSER_HOME:      return L"主页";
        case VK_VOLUME_MUTE:       return L"静音";
        case VK_VOLUME_DOWN:       return L"音量-";
        case VK_VOLUME_UP:         return L"音量+";
        case VK_MEDIA_NEXT_TRACK:  return L"下一曲";
        case VK_MEDIA_PREV_TRACK:  return L"上一曲";
        case VK_MEDIA_STOP:        return L"停止播放";
        case VK_MEDIA_PLAY_PAUSE:  return L"播放/暂停";
        case VK_LAUNCH_MAIL:       return L"邮件";
        case VK_LAUNCH_MEDIA_SELECT: return L"媒体播放器";
        case VK_LWIN:              return L"Win";
        case VK_RWIN:              return L"Win";
        case VK_APPS:              return L"菜单键";
        case VK_SNAPSHOT:          return L"PrintScreen";
        case VK_SCROLL:            return L"ScrollLock";
        case VK_PAUSE:             return L"Pause";
        case VK_NUMLOCK:           return L"NumLock";
        case VK_CAPITAL:           return L"CapsLock";
        case VK_LEFT:              return L"←";
        case VK_RIGHT:             return L"→";
        case VK_UP:                return L"↑";
        case VK_DOWN:              return L"↓";
        default:                   return nullptr;
    }
}

}  // namespace

const wchar_t* PresetDisplayName(PresetAction a) {
    switch (a) {
        case PresetAction::Copy:           return L"复制";
        case PresetAction::Paste:          return L"粘贴";
        case PresetAction::Cut:            return L"剪切";
        case PresetAction::Undo:           return L"撤销";
        case PresetAction::Redo:           return L"重做";
        case PresetAction::SelectAll:      return L"全选";
        case PresetAction::BrowserBack:    return L"浏览器后退";
        case PresetAction::BrowserForward: break;
    }
    return L"浏览器前进";
}

std::wstring PresetDisplayFull(PresetAction a) {
    std::wstring s = PresetDisplayName(a);
    s += L"：";
    s += HotkeyText(PresetHotkey(a));
    return s;
}

std::wstring KeyName(unsigned vk) {
    if (vk == 0) return L"";

    const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scan != 0) {
        LONG param = static_cast<LONG>(scan << 16);
        switch (vk) {
            case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
            case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
            case VK_INSERT: case VK_DELETE: case VK_NUMLOCK:
            case VK_DIVIDE: case VK_RETURN:
                param |= (1L << 24);  // 扩展键标志
                break;
            default:
                break;
        }
        wchar_t buf[64]{};
        if (GetKeyNameTextW(param, buf, static_cast<int>(std::size(buf))) > 0 && buf[0]) {
            return std::wstring(buf);
        }
    }
    if (const wchar_t* fallback = FallbackKeyName(vk)) return std::wstring(fallback);

    wchar_t buf[32]{};
    wsprintfW(buf, L"VK%02X", vk);
    return std::wstring(buf);
}

std::wstring HotkeyText(const Hotkey& hk) {
    if (hk.IsEmpty()) return L"（未设置）";
    std::wstring s;
    if (hk.ctrl)  s += L"Ctrl + ";
    if (hk.alt)   s += L"Alt + ";
    if (hk.shift) s += L"Shift + ";
    if (hk.win)   s += L"Win + ";
    s += KeyName(hk.vk);
    return s;
}

std::wstring MappingText(const Mapping& m) {
    switch (m.kind) {
        case ActionKind::Preset:
            return PresetDisplayName(m.preset) + (L"：" + HotkeyText(PresetHotkey(m.preset)));
        case ActionKind::Custom:
            return std::wstring(L"自定义：") + HotkeyText(m.hotkey);
        case ActionKind::Original:
            return L"保留鼠标原始功能";
        case ActionKind::Disabled:
            break;
    }
    return L"已禁用";
}

void Execute(const Mapping& m) {
    switch (m.kind) {
        case ActionKind::Preset:
            SendHotkey(PresetHotkey(m.preset));
            break;
        case ActionKind::Custom:
            if (!m.hotkey.IsEmpty()) SendHotkey(m.hotkey);
            break;
        case ActionKind::Original:
        case ActionKind::Disabled:
            break;
    }
}

}  // namespace keymap
}  // namespace ms
