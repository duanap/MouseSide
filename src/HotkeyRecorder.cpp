// HotkeyRecorder.cpp

#include "HotkeyRecorder.h"

#include <string>
#include <utility>

#include "AppInfo.h"
#include "KeyMapper.h"
#include "Utils.h"

namespace ms {
namespace {

constexpr wchar_t kRecorderClassName[] = L"MouseSide.HotkeyRecorder";

constexpr UINT kMsgCancel = WM_APP + 20;

constexpr int kIdPreview = 2001;

// 客户区逻辑尺寸（96 DPI 下的像素值）
constexpr int kClientWidth  = 360;
constexpr int kClientHeight = 168;

// 键盘钩子上报的是左右分离的修饰键，这里统一归一化
UINT NormalizeVirtualKey(UINT vk) noexcept {
    switch (vk) {
        case VK_CONTROL: return VK_LCONTROL;
        case VK_SHIFT:   return VK_LSHIFT;
        case VK_MENU:    return VK_LMENU;
        default:         return vk;
    }
}

std::wstring ModifierText(UINT mods) {
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
    return text;
}

}  // namespace

HotkeyRecorder* HotkeyRecorder::s_active = nullptr;

HotkeyRecorder::~HotkeyRecorder() {
    Cancel();
    RemoveKeyboardHook();
}

bool HotkeyRecorder::EnsureClass() {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize        = static_cast<UINT>(sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &HotkeyRecorder::WndProcThunk;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // 背景由 WM_ERASEBKGND 自行处理
    wc.lpszClassName = kRecorderClassName;
    wc.hIcon         = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(ms::kAppIconResourceId));
    wc.hIconSm       = wc.hIcon;

    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    registered = true;
    return true;
}

void HotkeyRecorder::Open(HWND owner, UINT initialMods, UINT initialVk,
                          ClosedCallback onClosed) {
    if (m_hwnd != nullptr) {
        SetForegroundWindow(m_hwnd);
        return;
    }
    if (!EnsureClass()) {
        return;
    }

    m_owner     = owner;
    m_onClosed  = std::move(onClosed);
    m_confirmed = false;
    m_wasActive = false;
    m_captured  = (initialVk != 0);
    m_mods      = initialMods;
    m_vk        = initialVk;
    for (bool& state : m_keyDown) {
        state = false;
    }

    if (m_background == nullptr) {
        m_background = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
    }

    // 先禁用 owner 再创建弹窗，避免弹窗被激活后又因 owner 状态变化而收到 WA_INACTIVE
    if (owner != nullptr) {
        EnableWindow(owner, FALSE);
    }

    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME,
                             kRecorderClassName,
                             L"录制快捷键",
                             WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             100, 100,
                             owner,
                             nullptr,
                             GetModuleHandleW(nullptr),
                             this);
    if (m_hwnd == nullptr) {
        if (owner != nullptr) {
            EnableWindow(owner, TRUE);
        }
        return;
    }

    CenterWindow(m_hwnd, owner);

    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
}

void HotkeyRecorder::ApplyLayout() {
    if (m_hwnd == nullptr) {
        return;
    }

    if (m_bodyFont != nullptr) {
        DeleteObject(m_bodyFont);
    }
    if (m_previewFont != nullptr) {
        DeleteObject(m_previewFont);
    }
    m_bodyFont    = ui::CreateUiFont(m_hwnd, 10, FW_NORMAL);
    m_previewFont = ui::CreateUiFont(m_hwnd, 16, FW_SEMIBOLD);

    // 先把窗口调整到目标客户区尺寸
    RECT desired{0, 0, ui::Scale(m_hwnd, kClientWidth), ui::Scale(m_hwnd, kClientHeight)};
    const DWORD style   = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE));
    if (AdjustWindowRectEx(&desired, style, FALSE, exStyle)) {
        SetWindowPos(m_hwnd, nullptr, 0, 0,
                     desired.right - desired.left,
                     desired.bottom - desired.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    const auto s = [this](int value) { return ui::Scale(m_hwnd, value); };

    MoveWindow(m_hintLabel,    s(20),  s(16),  s(320), s(20), TRUE);
    MoveWindow(m_preview,      s(20),  s(44),  s(320), s(46), TRUE);
    MoveWindow(m_okButton,     s(162), s(118), s(84),  s(30), TRUE);
    MoveWindow(m_cancelButton, s(256), s(118), s(84),  s(30), TRUE);

    SendMessageW(m_hintLabel,    WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    SendMessageW(m_preview,      WM_SETFONT, reinterpret_cast<WPARAM>(m_previewFont), TRUE);
    SendMessageW(m_okButton,     WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    SendMessageW(m_cancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
}

UINT HotkeyRecorder::CurrentMods() const {
    UINT mods = MOD_NONE;
    if (m_keyDown[VK_LCONTROL] || m_keyDown[VK_RCONTROL]) {
        mods |= MOD_CTRL;
    }
    if (m_keyDown[VK_LMENU] || m_keyDown[VK_RMENU]) {
        mods |= MOD_ALT;
    }
    if (m_keyDown[VK_LSHIFT] || m_keyDown[VK_RSHIFT]) {
        mods |= MOD_SHIFT;
    }
    if (m_keyDown[VK_LWIN] || m_keyDown[VK_RWIN]) {
        mods |= MOD_WIN;
    }
    return mods;
}

void HotkeyRecorder::ProcessKey(UINT vk, bool down) {
    if (m_hwnd == nullptr) {
        return;
    }

    vk = NormalizeVirtualKey(vk);

    if (down && vk == VK_ESCAPE) {
        // 不在钩子回调里直接销毁窗口，改为投递到消息循环
        PostMessageW(m_hwnd, kMsgCancel, 0, 0);
        return;
    }
    if (vk > 0xFF) {
        return;
    }

    if (down) {
        m_keyDown[vk] = true;
        if (IsModifierKey(vk)) {
            if (m_captured) {
                // 又开始按新的修饰键 -> 重新录制
                m_captured = false;
                m_vk       = 0;
                m_mods     = MOD_NONE;
            }
        } else {
            m_vk       = vk;
            m_mods     = CurrentMods();
            m_captured = true;
        }
    } else {
        m_keyDown[vk] = false;
    }

    UpdatePreview();
}

void HotkeyRecorder::UpdatePreview() {
    if (m_preview == nullptr) {
        return;
    }

    std::wstring text;
    if (m_captured && m_vk != 0) {
        text = HotkeyToString(m_mods, m_vk);
    } else if (CurrentMods() != MOD_NONE) {
        text = ModifierText(CurrentMods()) + L"…";
    } else {
        text = L"等待输入…";
    }

    SetWindowTextW(m_preview, text.c_str());

    if (m_okButton != nullptr) {
        EnableWindow(m_okButton, (m_captured && m_vk != 0) ? TRUE : FALSE);
    }

    InvalidateRect(m_preview, nullptr, TRUE);
    InvalidateRect(m_hintLabel, nullptr, TRUE);
}

void HotkeyRecorder::Confirm() {
    if (m_hwnd == nullptr || !m_captured || m_vk == 0) {
        return;
    }
    m_confirmed = true;

    const HWND window = m_hwnd;
    m_hwnd = nullptr;  // 先摘句柄，避免 DestroyWindow 期间的重入
    DestroyWindow(window);
}

void HotkeyRecorder::Cancel() {
    if (m_hwnd == nullptr) {
        return;
    }
    m_confirmed = false;

    const HWND window = m_hwnd;
    m_hwnd = nullptr;
    DestroyWindow(window);
}

void HotkeyRecorder::InstallKeyboardHook() {
    if (m_keyboardHook != nullptr) {
        return;
    }

    s_active = this;
    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL,
                                       &HotkeyRecorder::KeyboardProc,
                                       GetModuleHandleW(nullptr),
                                       0);
    if (m_keyboardHook == nullptr) {
        s_active = nullptr;
        OutputDebugStringW(L"[MouseSide] 录制期间未能安装键盘钩子，改用窗口消息兜底\n");
    }
}

void HotkeyRecorder::RemoveKeyboardHook() {
    if (m_keyboardHook != nullptr) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
    if (s_active == this) {
        s_active = nullptr;
    }
}

LRESULT CALLBACK HotkeyRecorder::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    HotkeyRecorder* self = s_active;

    if (nCode == HC_ACTION && self != nullptr && self->m_hwnd != nullptr) {
        // 只有录制窗口处于前台时才吞键。
        // 这样即便出现异常（窗口被抢焦点、录制会话未正常结束），
        // 键盘也不会被长时间阻塞。
        if (GetForegroundWindow() == self->m_hwnd) {
            const bool isDown = (wParam == WM_KEYDOWN) || (wParam == WM_SYSKEYDOWN);
            const bool isUp   = (wParam == WM_KEYUP) || (wParam == WM_SYSKEYUP);
            if (isDown || isUp) {
                const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
                self->ProcessKey(info->vkCode, isDown);
                return 1;  // 吞掉原始按键，避免 Win / Alt+F4 等产生副作用
            }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK HotkeyRecorder::WndProcThunk(HWND hwnd, UINT message,
                                              WPARAM wParam, LPARAM lParam) {
    HotkeyRecorder* self =
        reinterpret_cast<HotkeyRecorder*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<HotkeyRecorder*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self != nullptr) {
        return self->HandleMessage(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT HotkeyRecorder::HandleMessage(HWND hwnd, UINT message,
                                      WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            // WM_CREATE 发生在 CreateWindowExW 返回之前，此处先补上窗口句柄
            if (m_hwnd == nullptr) {
                m_hwnd = hwnd;
            }

            m_hintLabel = CreateWindowExW(0, L"STATIC", L"请按下要映射的快捷键组合（Esc 取消）",
                                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                                          0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            m_preview = CreateWindowExW(0, L"STATIC", L"",
                                        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                                        0, 0, 0, 0, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPreview)),
                                        nullptr, nullptr);

            m_okButton = CreateWindowExW(0, L"BUTTON", L"保存",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                         0, 0, 0, 0, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                                         nullptr, nullptr);

            m_cancelButton = CreateWindowExW(0, L"BUTTON", L"取消",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                             0, 0, 0, 0, hwnd,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                                             nullptr, nullptr);

            ApplyLayout();
            UpdatePreview();
            InstallKeyboardHook();
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            if (reinterpret_cast<HWND>(lParam) == m_preview) {
                SetTextColor(dc, m_captured ? RGB(0x1A, 0x53, 0xD8)
                                            : RGB(0x9A, 0xA0, 0xA6));
            } else {
                SetTextColor(dc, RGB(0x50, 0x56, 0x5E));
            }
            return reinterpret_cast<LRESULT>(m_background);
        }

        case WM_ERASEBKGND: {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, m_background);
            return 1;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    Confirm();
                    return 0;
                case IDCANCEL:
                    Cancel();
                    return 0;
                default:
                    break;
            }
            break;

        case kMsgCancel:
            Cancel();
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // 键盘钩子不可用时的兜底路径（钩子可用时事件到不了这里）
            ProcessKey(static_cast<UINT>(wParam), true);
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            ProcessKey(static_cast<UINT>(wParam), false);
            return 0;

        case WM_SYSCHAR:
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0u) == SC_KEYMENU) {
                return 0;
            }
            break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                // 失去焦点即放弃录制；但要避开窗口刚创建时可能出现的一次假失活
                if (m_wasActive) {
                    Cancel();
                }
            } else {
                m_wasActive = true;
            }
            return 0;

        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested != nullptr) {
                SetWindowPos(hwnd, nullptr,
                             suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            ApplyLayout();
            return 0;
        }

        case WM_CLOSE:
            Cancel();
            return 0;

        case WM_DESTROY: {
            RemoveKeyboardHook();

            if (m_bodyFont != nullptr) {
                DeleteObject(m_bodyFont);
                m_bodyFont = nullptr;
            }
            if (m_previewFont != nullptr) {
                DeleteObject(m_previewFont);
                m_previewFont = nullptr;
            }
            if (m_background != nullptr) {
                DeleteObject(m_background);
                m_background = nullptr;
            }

            m_hintLabel    = nullptr;
            m_preview      = nullptr;
            m_okButton     = nullptr;
            m_cancelButton = nullptr;
            m_hwnd         = nullptr;
            m_wasActive    = false;

            if (m_owner != nullptr) {
                EnableWindow(m_owner, TRUE);
                SetForegroundWindow(m_owner);
                m_owner = nullptr;
            }

            ClosedCallback callback = std::move(m_onClosed);
            m_onClosed = nullptr;

            if (callback) {
                callback(m_confirmed, m_mods, m_vk);
            }
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace ms
