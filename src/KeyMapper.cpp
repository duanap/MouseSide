// KeyMapper.cpp

#include "KeyMapper.h"

#include <cwchar>
#include <string.h>

#include <iterator>
#include <string>
#include <vector>

namespace ms {
namespace {

struct PresetKeys {
    UINT mods;
    UINT vk;
};

// 预设动作对应的快捷键；Back / Forward 走鼠标事件，不在此表内
bool TryGetPresetKeys(ActionType type, PresetKeys& keys) noexcept {
    switch (type) {
        case ActionType::Paste:     keys = PresetKeys{MOD_CTRL, 'V'}; return true;
        case ActionType::Copy:      keys = PresetKeys{MOD_CTRL, 'C'}; return true;
        case ActionType::Cut:       keys = PresetKeys{MOD_CTRL, 'X'}; return true;
        case ActionType::Undo:      keys = PresetKeys{MOD_CTRL, 'Z'}; return true;
        case ActionType::Redo:      keys = PresetKeys{MOD_CTRL, 'Y'}; return true;
        case ActionType::SelectAll: keys = PresetKeys{MOD_CTRL, 'A'}; return true;
        default: return false;
    }
}

void AppendKeyEvent(std::vector<INPUT>& out, UINT vk, bool keyDown) {
    INPUT in{};
    in.type       = INPUT_KEYBOARD;
    in.ki.wVk     = static_cast<WORD>(vk);
    in.ki.wScan   = 0;
    in.ki.dwFlags = keyDown ? 0u : KEYEVENTF_KEYUP;
    if (IsExtendedKey(vk)) {
        in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    in.ki.time        = 0;
    in.ki.dwExtraInfo = kInjectedMarker;
    out.push_back(in);
}

std::wstring ToHexKey(UINT vk) {
    static const wchar_t* kDigits = L"0123456789ABCDEF";
    std::wstring text = L"0x";
    bool started = false;
    for (int shift = 28; shift >= 0; shift -= 4) {
        const UINT nibble = (vk >> shift) & 0xFu;
        if (nibble != 0 || started) {
            text.push_back(kDigits[nibble]);
            started = true;
        }
    }
    if (!started) {
        text.push_back(L'0');
    }
    return text;
}

}  // namespace

// ---------------------------------------------------------------------------
// 修饰键 / 扩展键判定
// ---------------------------------------------------------------------------

bool IsModifierKey(UINT vk) noexcept {
    switch (vk) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

bool IsExtendedKey(UINT vk) noexcept {
    switch (vk) {
        case VK_RMENU:
        case VK_RCONTROL:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
        case VK_APPS:
        case VK_LWIN:
        case VK_RWIN:
        case VK_SNAPSHOT:
        case VK_CLEAR:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// 序列化 / 显示
// ---------------------------------------------------------------------------

const wchar_t* ActionTypeName(ActionType type) noexcept {
    switch (type) {
        case ActionType::Paste:        return L"Paste";
        case ActionType::Copy:         return L"Copy";
        case ActionType::Cut:          return L"Cut";
        case ActionType::Undo:         return L"Undo";
        case ActionType::Redo:         return L"Redo";
        case ActionType::SelectAll:    return L"SelectAll";
        case ActionType::Back:         return L"Back";
        case ActionType::Forward:      return L"Forward";
        case ActionType::Custom:       return L"Custom";
        case ActionType::KeepOriginal: return L"KeepOriginal";
        case ActionType::Disabled:     return L"Disabled";
    }
    return L"KeepOriginal";
}

ActionType ActionTypeFromName(const std::wstring& name, ActionType fallback) noexcept {
    static const struct {
        const wchar_t* name;
        ActionType     type;
    } kTable[] = {
        {L"Paste",        ActionType::Paste},
        {L"Copy",         ActionType::Copy},
        {L"Cut",          ActionType::Cut},
        {L"Undo",         ActionType::Undo},
        {L"Redo",         ActionType::Redo},
        {L"SelectAll",    ActionType::SelectAll},
        {L"Back",         ActionType::Back},
        {L"Forward",      ActionType::Forward},
        {L"Custom",       ActionType::Custom},
        {L"KeepOriginal", ActionType::KeepOriginal},
        {L"Disabled",     ActionType::Disabled},
    };
    for (const auto& entry : kTable) {
        if (_wcsicmp(name.c_str(), entry.name) == 0) {
            return entry.type;
        }
    }
    return fallback;
}

const wchar_t* ActionTypeLabel(ActionType type) noexcept {
    switch (type) {
        case ActionType::Paste:        return L"粘贴";
        case ActionType::Copy:         return L"复制";
        case ActionType::Cut:          return L"剪切";
        case ActionType::Undo:         return L"撤销";
        case ActionType::Redo:         return L"重做";
        case ActionType::SelectAll:    return L"全选";
        case ActionType::Back:         return L"浏览器后退";
        case ActionType::Forward:      return L"浏览器前进";
        case ActionType::Custom:       return L"自定义快捷键";
        case ActionType::KeepOriginal: return L"保留鼠标原始功能";
        case ActionType::Disabled:     return L"已禁用";
    }
    return L"保留鼠标原始功能";
}

bool ActionPresetKeys(ActionType type, UINT& mods, UINT& vk) noexcept {
    PresetKeys keys{0, 0};
    if (!TryGetPresetKeys(type, keys)) {
        return false;
    }
    mods = keys.mods;
    vk   = keys.vk;
    return true;
}

std::wstring ActionSummary(const MappedAction& action) {
    UINT mods = 0;
    UINT vk   = 0;
    if (ActionPresetKeys(action.type, mods, vk)) {
        return HotkeyToString(mods, vk);
    }
    switch (action.type) {
        case ActionType::Custom:
            return action.vk != 0 ? HotkeyToString(action.mods, action.vk) : L"尚未录制";
        case ActionType::Back:
            return L"重放鼠标后退事件";
        case ActionType::Forward:
            return L"重放鼠标前进事件";
        case ActionType::KeepOriginal:
            return L"不拦截，直接放行";
        case ActionType::Disabled:
            return L"点击无任何操作";
        default:
            return std::wstring();
    }
}

std::wstring VirtualKeyName(UINT vk) {
    switch (vk) {
        case VK_BACK:     return L"Backspace";
        case VK_TAB:      return L"Tab";
        case VK_CLEAR:    return L"Clear";
        case VK_RETURN:   return L"Enter";
        case VK_PAUSE:    return L"Pause";
        case VK_CAPITAL:  return L"CapsLock";
        case VK_ESCAPE:   return L"Esc";
        case VK_SPACE:    return L"Space";
        case VK_PRIOR:    return L"PageUp";
        case VK_NEXT:     return L"PageDown";
        case VK_END:      return L"End";
        case VK_HOME:     return L"Home";
        case VK_LEFT:     return L"←";
        case VK_UP:       return L"↑";
        case VK_RIGHT:    return L"→";
        case VK_DOWN:     return L"↓";
        case VK_INSERT:   return L"Insert";
        case VK_DELETE:   return L"Delete";
        case VK_SNAPSHOT: return L"PrintScreen";
        case VK_NUMLOCK:  return L"NumLock";
        case VK_SCROLL:   return L"ScrollLock";
        case VK_LWIN:     return L"Win";
        case VK_RWIN:     return L"Win";
        case VK_APPS:     return L"Menu";
        case VK_MULTIPLY: return L"Num *";
        case VK_ADD:      return L"Num +";
        case VK_SUBTRACT: return L"Num -";
        case VK_DECIMAL:  return L"Num .";
        case VK_DIVIDE:   return L"Num /";
        case VK_OEM_1:      return L";";
        case VK_OEM_PLUS:   return L"=";
        case VK_OEM_COMMA:  return L",";
        case VK_OEM_MINUS:  return L"-";
        case VK_OEM_PERIOD: return L".";
        case VK_OEM_2:      return L"/";
        case VK_OEM_3:      return L"`";
        case VK_OEM_4:      return L"[";
        case VK_OEM_5:      return L"\\";
        case VK_OEM_6:      return L"]";
        case VK_OEM_7:      return L"'";
        default:
            break;
    }

    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return L"Num " + std::to_wstring(vk - VK_NUMPAD0);
    }
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }

    // 兜底：交给系统按键名表
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scanCode != 0) {
        LONG lParam = static_cast<LONG>(scanCode << 16);
        if (IsExtendedKey(vk)) {
            lParam |= (1L << 24);
        }
        wchar_t buffer[64] = {};
        if (GetKeyNameTextW(lParam, buffer, static_cast<int>(std::size(buffer))) > 0) {
            return buffer;
        }
    }
    return ToHexKey(vk);
}

std::wstring HotkeyToString(UINT mods, UINT vk) {
    std::wstring text;
    if ((mods & MOD_CTRL) != 0) {
        text += L"Ctrl + ";
    }
    if ((mods & MOD_ALT) != 0) {
        text += L"Alt + ";
    }
    if ((mods & MOD_SHIFT) != 0) {
        text += L"Shift + ";
    }
    if ((mods & MOD_WIN) != 0) {
        text += L"Win + ";
    }
    text += VirtualKeyName(vk);
    return text;
}

// ---------------------------------------------------------------------------
// 注入
// ---------------------------------------------------------------------------

void SendKeyStroke(UINT vk, UINT mods) {
    if (vk == 0) {
        return;
    }

    std::vector<INPUT> sequence;
    sequence.reserve(8);

    if ((mods & MOD_CTRL) != 0) {
        AppendKeyEvent(sequence, VK_CONTROL, true);
    }
    if ((mods & MOD_ALT) != 0) {
        AppendKeyEvent(sequence, VK_MENU, true);
    }
    if ((mods & MOD_SHIFT) != 0) {
        AppendKeyEvent(sequence, VK_SHIFT, true);
    }
    if ((mods & MOD_WIN) != 0) {
        AppendKeyEvent(sequence, VK_LWIN, true);
    }

    AppendKeyEvent(sequence, vk, true);
    AppendKeyEvent(sequence, vk, false);

    if ((mods & MOD_WIN) != 0) {
        AppendKeyEvent(sequence, VK_LWIN, false);
    }
    if ((mods & MOD_SHIFT) != 0) {
        AppendKeyEvent(sequence, VK_SHIFT, false);
    }
    if ((mods & MOD_ALT) != 0) {
        AppendKeyEvent(sequence, VK_MENU, false);
    }
    if ((mods & MOD_CTRL) != 0) {
        AppendKeyEvent(sequence, VK_CONTROL, false);
    }

    const UINT sent = SendInput(static_cast<UINT>(sequence.size()),
                                sequence.data(),
                                static_cast<int>(sizeof(INPUT)));
    if (sent != sequence.size()) {
        // 目标窗口权限高于本进程时会被 UIPI 静默丢弃，重试一次无意义
        OutputDebugStringW(L"[MouseSide] SendInput 未能完整注入键盘事件\n");
    }
}

void SendMouseXButton(bool forward) {
    INPUT events[2] = {};

    for (int i = 0; i < 2; ++i) {
        events[i].type           = INPUT_MOUSE;
        events[i].mi.dx          = 0;
        events[i].mi.dy          = 0;
        events[i].mi.mouseData   = forward ? XBUTTON2 : XBUTTON1;
        events[i].mi.dwFlags     = (i == 0) ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        events[i].mi.time        = 0;
        events[i].mi.dwExtraInfo = kInjectedMarker;
    }

    const UINT sent = SendInput(2, events, static_cast<int>(sizeof(INPUT)));
    if (sent != 2) {
        OutputDebugStringW(L"[MouseSide] SendInput 未能注入鼠标事件\n");
    }
}

bool ExecuteAction(const MappedAction& action) {
    UINT presetMods = 0;
    UINT presetVk   = 0;

    if (ActionPresetKeys(action.type, presetMods, presetVk)) {
        SendKeyStroke(presetVk, presetMods);
        return true;
    }

    switch (action.type) {
        case ActionType::Back:
            SendMouseXButton(false);
            return true;
        case ActionType::Forward:
            SendMouseXButton(true);
            return true;
        case ActionType::Custom:
            if (action.vk == 0) {
                return false;
            }
            SendKeyStroke(action.vk, action.mods);
            return true;
        case ActionType::KeepOriginal:
        case ActionType::Disabled:
            // 两者都不会进入执行路径（由钩子层处理），此处保持幂等
            return true;
        default:
            return false;
    }
}

}  // namespace ms
