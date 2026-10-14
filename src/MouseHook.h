// MouseHook.h
//
// 模块 1 / 4：鼠标侧键监听
//
//   - 基于 WH_MOUSE_LL 全局低层鼠标钩子
//   - 只关心 XBUTTON1 / XBUTTON2（第四、第五按钮）
//   - 钩子回调内不做任何耗时操作，只负责「吞事件 + 抛通知」
//   - 未配置映射、未启用或处于暂停状态时，原始事件一律放行

#pragma once

#include <windows.h>

#include <functional>
#include <utility>

namespace ms {

class MouseHook {
public:
    // 触发回调。在安装钩子的线程（UI 线程）上执行，实现方必须保持轻量。
    using TriggerCallback = std::function<void(int buttonIndex)>;

    MouseHook() = default;
    ~MouseHook();

    MouseHook(const MouseHook&)            = delete;
    MouseHook& operator=(const MouseHook&) = delete;

    // 安装全局低层鼠标钩子
    bool Install();

    // 卸载钩子并释放资源
    void Uninstall();

    // 钩子是否处于已安装状态
    bool IsInstalled() const noexcept { return m_hook != nullptr; }

    // 重建钩子。仅用于极端情况下系统摘除钩子后的自愈，调用方应保持低频。
    bool Refresh();

    void SetTriggerCallback(TriggerCallback callback) { m_onTrigger = std::move(callback); }

    // 热路径开关：只有 intercept 为 true 的按钮才会被拦截并回调。
    // 由 UI 线程在配置变化时更新，钩子回调只做一次数组读取。
    void SetIntercept(int buttonIndex, bool intercept) noexcept;

    void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
    bool IsEnabled() const noexcept { return m_enabled; }

    // 最近一次安装失败的错误码（ERROR_SUCCESS 表示正常）
    DWORD LastError() const noexcept { return m_lastError; }

private:
    static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);

    HHOOK  m_hook      = nullptr;
    DWORD  m_lastError = ERROR_SUCCESS;
    bool   m_enabled   = false;
    bool   m_intercept[2] = {false, false};
    TriggerCallback m_onTrigger;

    // WH_MOUSE_LL 的回调是自由函数，用单例指针回到对象实例。
    // 本程序只允许存在一个 MouseHook 实例。
    static MouseHook* s_active;
};

}  // namespace ms
