// HotkeyRecorder.h
//
// 自定义快捷键录制器。
//
//   交互流程：
//     打开录制窗口 -> 临时挂上 WH_KEYBOARD_LL 捕获一次组合键
//     -> 预览实时显示 -> 用户点「保存」确认，或按 Esc / 点「取消」放弃
//
//   录制期间为了不产生副作用（例如 Win 键弹出开始菜单、Alt+F4 关窗），
//   会吞掉所有键盘事件；仅当录制窗口处于前台时才吞，避免异常情况下卡住键盘。

#pragma once

#include <windows.h>

#include <functional>

namespace ms {

class HotkeyRecorder {
public:
    // confirmed = true 表示用户按下了「保存」；false 表示取消 / 放弃
    using ClosedCallback = std::function<void(bool confirmed, UINT mods, UINT vk)>;

    HotkeyRecorder() = default;
    ~HotkeyRecorder();

    HotkeyRecorder(const HotkeyRecorder&)            = delete;
    HotkeyRecorder& operator=(const HotkeyRecorder&) = delete;

    // 打开录制窗口。owner 会被临时禁用（伪模态）。
    // 窗口关闭时回调 onClosed，无论确认还是取消都会触发。
    void Open(HWND owner, UINT initialMods, UINT initialVk, ClosedCallback onClosed);

    bool IsOpen() const noexcept { return m_hwnd != nullptr; }

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool EnsureClass();
    void ApplyLayout();
    void ProcessKey(UINT vk, bool down);
    void UpdatePreview();
    void Confirm();
    void Cancel();
    void InstallKeyboardHook();
    void RemoveKeyboardHook();
    UINT CurrentMods() const;

    static HotkeyRecorder* s_active;

    HHOOK m_keyboardHook = nullptr;
    HWND  m_hwnd         = nullptr;
    HWND  m_owner        = nullptr;
    HWND  m_hintLabel    = nullptr;
    HWND  m_preview      = nullptr;
    HWND  m_okButton     = nullptr;
    HWND  m_cancelButton = nullptr;

    HFONT m_bodyFont    = nullptr;
    HFONT m_previewFont = nullptr;
    HBRUSH m_background = nullptr;

    bool m_keyDown[256] = {};
    bool m_captured     = false;
    UINT m_mods         = 0;
    UINT m_vk           = 0;
    bool m_confirmed    = false;
    // 窗口是否至少被激活过一次。
    // 用于避免 SetForegroundWindow 失败时收到首个 WA_INACTIVE 就立刻自我取消。
    bool m_wasActive    = false;

    ClosedCallback m_onClosed;
};

}  // namespace ms
