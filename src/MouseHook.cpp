// MouseHook.cpp

#include "MouseHook.h"

#include "KeyMapper.h"

namespace ms {

MouseHook* MouseHook::s_active = nullptr;

MouseHook::~MouseHook() {
    Uninstall();
}

void MouseHook::SetIntercept(int buttonIndex, bool intercept) noexcept {
    if (buttonIndex >= 0 && buttonIndex < 2) {
        m_intercept[buttonIndex] = intercept;
    }
}

LRESULT CALLBACK MouseHook::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    MouseHook* self = s_active;

    if (nCode == HC_ACTION && self != nullptr && self->m_enabled) {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);

        // 放行本程序自己注入的事件。
        // 「浏览器后退 / 前进」动作需要重放 XBUTTON 事件，若不识别标记会形成递归拦截。
        if (info->dwExtraInfo != kInjectedMarker) {
            const bool isDown = (wParam == WM_XBUTTONDOWN) || (wParam == WM_NCXBUTTONDOWN);
            const bool isUp   = (wParam == WM_XBUTTONUP)   || (wParam == WM_NCXBUTTONUP);

            if (isDown || isUp) {
                const WORD xButton = HIWORD(info->mouseData);

                int index = -1;
                if (xButton == XBUTTON1) {
                    index = 0;  // 第四按钮
                } else if (xButton == XBUTTON2) {
                    index = 1;  // 第五按钮
                }

                if (index >= 0 && self->m_intercept[index]) {
                    if (isDown && self->m_onTrigger) {
                        // 只做「投递」，实际注入交给消息循环，保证回调保持轻量
                        self->m_onTrigger(index);
                    }
                    // 按下与松开一并吞掉，彻底屏蔽原始前进 / 后退行为
                    return 1;
                }
            }
        }
    }

    // 其余事件（移动、左中右键、滚轮等）原样放行
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool MouseHook::Install() {
    if (m_hook != nullptr) {
        return true;
    }

    s_active = this;
    m_hook = SetWindowsHookExW(WH_MOUSE_LL,
                               &MouseHook::HookProc,
                               GetModuleHandleW(nullptr),
                               0);
    if (m_hook == nullptr) {
        m_lastError = GetLastError();
        s_active    = nullptr;
        return false;
    }

    m_lastError = ERROR_SUCCESS;
    return true;
}

void MouseHook::Uninstall() {
    if (m_hook != nullptr) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (s_active == this) {
        s_active = nullptr;
    }
}

bool MouseHook::Refresh() {
    if (m_hook == nullptr) {
        return false;  // 未安装时不自作主张地安装
    }
    Uninstall();
    return Install();
}

}  // namespace ms
