// TrayUI.cpp

#include "TrayUI.h"

#include <commctrl.h>

#include <string>

#include "App.h"
#include "AppInfo.h"
#include "Config.h"
#include "IconFactory.h"
#include "KeyMapper.h"
#include "Utils.h"

namespace ms {
namespace {

// ---------------------------------------------------------------------------
// 托盘
// ---------------------------------------------------------------------------

constexpr UINT kTrayIconId = 1;

constexpr UINT kMenuTrayOpen   = 20001;
constexpr UINT kMenuTrayToggle = 20002;
constexpr UINT kMenuTrayExit   = 20003;

// ---------------------------------------------------------------------------
// 侧键动作菜单
// ---------------------------------------------------------------------------

constexpr UINT kMenuPresetBase   = 1000;
constexpr UINT kMenuCustom       = 1020;
constexpr UINT kMenuKeepOriginal = 1021;
constexpr UINT kMenuDisabled     = 1022;

const ActionType kPresetMenuActions[] = {
    ActionType::Paste,
    ActionType::Copy,
    ActionType::Cut,
    ActionType::Undo,
    ActionType::Redo,
    ActionType::SelectAll,
    ActionType::Back,
    ActionType::Forward,
};

// ---------------------------------------------------------------------------
// 设置窗口控件 ID
// ---------------------------------------------------------------------------

constexpr int kIdEditButtonBase = 3000;
constexpr int kIdCardBase       = 3010;
constexpr int kIdEnableCheck    = 3020;
constexpr int kIdAutoStartCheck = 3021;
constexpr int kIdResetButton    = 3022;

// ---------------------------------------------------------------------------
// 布局常量（96 DPI 下的逻辑像素）
// ---------------------------------------------------------------------------

constexpr int kClientWidth  = 400;
constexpr int kClientHeight = 364;

constexpr int kMargin      = 20;
constexpr int kCardHeight  = 66;
constexpr int kCardTop[2]  = {80, 154};
constexpr int kEditButtonW = 68;
constexpr int kEditButtonH = 30;

// 钩子自愈定时器：间隔较长，避免产生「持续轮询」的开销
constexpr UINT_PTR kHookWatchdogTimerId = 1;
constexpr UINT     kHookWatchdogIntervalMs = 120000;

UINT MenuFlags(bool checked, bool disabled = false) {
    UINT flags = static_cast<UINT>(MF_STRING);
    if (checked) {
        flags |= static_cast<UINT>(MF_CHECKED);
    }
    if (disabled) {
        flags |= static_cast<UINT>(MF_GRAYED);
    }
    return flags;
}

std::wstring ButtonDisplayName(int index) {
    return index == 0 ? L"鼠标第四按钮 [4]" : L"鼠标第五按钮 [5]";
}

// 菜单项文字，把快捷键用 \t 分隔以获得右对齐的加速键提示
std::wstring PresetMenuText(ActionType type) {
    std::wstring text = ActionTypeLabel(type);
    UINT mods = 0;
    UINT vk   = 0;
    if (ActionPresetKeys(type, mods, vk)) {
        text += L"\t";
        text += HotkeyToString(mods, vk);
    }
    return text;
}

}  // namespace

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------

TrayUI::TrayUI() = default;

TrayUI::~TrayUI() {
    Destroy();
}

bool TrayUI::Create(HINSTANCE instance, App* app) {
    m_instance = instance;
    m_app      = app;

    m_windowBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    m_cardBrush   = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

    if (!EnsureWindowClasses()) {
        return false;
    }
    if (!CreateMessageWindow()) {
        return false;
    }
    if (!CreateSettingsWindow()) {
        return false;
    }

    AddTrayIcon();

    // 任务栏重启（explorer.exe 崩溃或重启）后需要重新添加托盘图标
    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    SetTimer(m_messageWindow, kHookWatchdogTimerId, kHookWatchdogIntervalMs, nullptr);

    return true;
}

void TrayUI::Destroy() {
    m_recorder.Cancel();

    if (m_messageWindow != nullptr) {
        KillTimer(m_messageWindow, kHookWatchdogTimerId);
    }
    RemoveTrayIcon();

    if (m_settingsWindow != nullptr) {
        DestroyWindow(m_settingsWindow);
        m_settingsWindow = nullptr;
    }
    if (m_messageWindow != nullptr) {
        DestroyWindow(m_messageWindow);
        m_messageWindow = nullptr;
    }

    DestroyFonts();

    if (m_iconRunning != nullptr) {
        DestroyIcon(m_iconRunning);
        m_iconRunning = nullptr;
    }
    if (m_iconPaused != nullptr) {
        DestroyIcon(m_iconPaused);
        m_iconPaused = nullptr;
    }
    if (m_windowBrush != nullptr) {
        DeleteObject(m_windowBrush);
        m_windowBrush = nullptr;
    }
    if (m_cardBrush != nullptr) {
        DeleteObject(m_cardBrush);
        m_cardBrush = nullptr;
    }

    m_taskbarCreatedMessage = 0;
    m_app                   = nullptr;
}

void TrayUI::DestroyFonts() {
    HFONT* fonts[] = {&m_titleFont, &m_boldFont, &m_bodyFont, &m_smallFont};
    for (HFONT* font : fonts) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

// ---------------------------------------------------------------------------
// 窗口类注册 / 创建
// ---------------------------------------------------------------------------

bool TrayUI::EnsureWindowClasses() {
    WNDCLASSEXW messageClass{};
    messageClass.cbSize        = static_cast<UINT>(sizeof(messageClass));
    messageClass.lpfnWndProc   = &TrayUI::MessageWindowProc;
    messageClass.hInstance     = m_instance;
    messageClass.hIcon         = LoadIconW(m_instance, MAKEINTRESOURCEW(kAppIconResourceId));
    messageClass.hIconSm       = messageClass.hIcon;
    messageClass.lpszClassName = kMessageWindowClass;

    if (RegisterClassExW(&messageClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW settingsClass{};
    settingsClass.cbSize        = static_cast<UINT>(sizeof(settingsClass));
    settingsClass.style         = CS_HREDRAW | CS_VREDRAW;
    settingsClass.lpfnWndProc   = &TrayUI::SettingsWindowProc;
    settingsClass.hInstance     = m_instance;
    settingsClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = nullptr;  // 背景在 WM_ERASEBKGND 中统一处理
    settingsClass.lpszClassName = kSettingsWindowClass;
    settingsClass.hIcon         = LoadIconW(m_instance, MAKEINTRESOURCEW(kAppIconResourceId));
    settingsClass.hIconSm       = settingsClass.hIcon;

    if (RegisterClassExW(&settingsClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW cardClass{};
    cardClass.cbSize        = static_cast<UINT>(sizeof(cardClass));
    cardClass.style         = CS_HREDRAW | CS_VREDRAW;
    cardClass.lpfnWndProc   = &TrayUI::CardWindowProc;
    cardClass.hInstance     = m_instance;
    cardClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    cardClass.hbrBackground = nullptr;
    cardClass.lpszClassName = kCardWindowClass;

    if (RegisterClassExW(&cardClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    return true;
}

bool TrayUI::CreateMessageWindow() {
    m_messageWindow = CreateWindowExW(0,
                                      kMessageWindowClass,
                                      kAppName,
                                      0,
                                      0, 0, 0, 0,
                                      nullptr, nullptr, m_instance, this);
    return m_messageWindow != nullptr;
}

bool TrayUI::CreateSettingsWindow() {
    m_settingsWindow = CreateWindowExW(0,
                                       kSettingsWindowClass,
                                       L"MouseSide 设置",
                                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       100, 100,
                                       nullptr, nullptr, m_instance, this);
    return m_settingsWindow != nullptr;
}

// ---------------------------------------------------------------------------
// 消息窗口
// ---------------------------------------------------------------------------

LRESULT CALLBACK TrayUI::MessageWindowProc(HWND hwnd, UINT message,
                                           WPARAM wParam, LPARAM lParam) {
    TrayUI* self = reinterpret_cast<TrayUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<TrayUI*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self != nullptr) {
        return self->HandleMessageWindow(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT TrayUI::HandleMessageWindow(HWND hwnd, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
    if (message == m_taskbarCreatedMessage && m_taskbarCreatedMessage != 0) {
        m_trayIconAdded = false;
        AddTrayIcon();
        return 0;
    }

    switch (message) {
        case kMsgTriggerAction:
            OnTriggerAction(static_cast<int>(wParam));
            return 0;

        case kMsgShowSettings:
            ShowSettings();
            return 0;

        case kMsgTrayNotify:
            HandleTrayMessage(lParam);
            return 0;

        case WM_TIMER:
            if (wParam == kHookWatchdogTimerId) {
                OnHookWatchdog();
                return 0;
            }
            break;

        case WM_DESTROY:
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void TrayUI::OnTriggerAction(int buttonIndex) {
    if (m_app == nullptr) {
        return;
    }
    m_app->TriggerAction(buttonIndex);
}

void TrayUI::OnHookWatchdog() {
    if (m_app == nullptr) {
        return;
    }
    m_app->RefreshHook();
}

// ---------------------------------------------------------------------------
// 托盘图标
// ---------------------------------------------------------------------------

void TrayUI::AddTrayIcon() {
    if (m_messageWindow == nullptr || m_trayIconAdded) {
        return;
    }

    const bool enabled = (m_app != nullptr) && m_app->IsEnabled();
    if (m_iconRunning == nullptr) {
        m_iconRunning = IconFactory::CreateTrayIcon(false);
    }
    if (m_iconPaused == nullptr) {
        m_iconPaused = IconFactory::CreateTrayIcon(true);
    }

    NOTIFYICONDATAW data{};
    data.cbSize           = static_cast<DWORD>(sizeof(data));
    data.hWnd             = m_messageWindow;
    data.uID              = kTrayIconId;
    data.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kMsgTrayNotify;
    data.hIcon            = enabled ? m_iconRunning : m_iconPaused;

    const std::wstring tip = std::wstring(kAppName) +
                             (enabled ? L" · 侧键映射已启用" : L" · 侧键映射已暂停");
    lstrcpynW(data.szTip, tip.c_str(), static_cast<int>(ARRAYSIZE(data.szTip)));

    m_trayIconAdded = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
}

void TrayUI::RemoveTrayIcon() {
    if (!m_trayIconAdded || m_messageWindow == nullptr) {
        m_trayIconAdded = false;
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = static_cast<DWORD>(sizeof(data));
    data.hWnd   = m_messageWindow;
    data.uID    = kTrayIconId;

    Shell_NotifyIconW(NIM_DELETE, &data);
    m_trayIconAdded = false;
}

void TrayUI::RefreshTrayIcon() {
    if (!m_trayIconAdded || m_messageWindow == nullptr) {
        return;
    }

    const bool enabled = (m_app != nullptr) && m_app->IsEnabled();

    NOTIFYICONDATAW data{};
    data.cbSize = static_cast<DWORD>(sizeof(data));
    data.hWnd   = m_messageWindow;
    data.uID    = kTrayIconId;
    data.uFlags = NIF_ICON | NIF_TIP;
    data.hIcon  = enabled ? m_iconRunning : m_iconPaused;

    const std::wstring tip = std::wstring(kAppName) +
                             (enabled ? L" · 侧键映射已启用" : L" · 侧键映射已暂停");
    lstrcpynW(data.szTip, tip.c_str(), static_cast<int>(ARRAYSIZE(data.szTip)));

    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayUI::HandleTrayMessage(LPARAM lParam) {
    switch (LOWORD(lParam)) {
        case WM_LBUTTONDBLCLK:
            ShowSettings();
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu();
            break;
        default:
            break;
    }
}

void TrayUI::ShowTrayMenu() {
    if (m_messageWindow == nullptr || m_app == nullptr) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const bool enabled = m_app->IsEnabled();

    AppendMenuW(menu, MF_STRING, kMenuTrayOpen, L"打开设置(&O)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MenuFlags(false), kMenuTrayToggle,
                enabled ? L"暂停侧键映射(&P)" : L"启用侧键映射(&E)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuTrayExit, L"退出(&X)");

    SetMenuDefaultItem(menu, kMenuTrayOpen, FALSE);

    POINT cursor{};
    GetCursorPos(&cursor);

    SetForegroundWindow(m_messageWindow);
    const UINT command = TrackPopupMenu(menu,
                                        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                        cursor.x, cursor.y, 0, m_messageWindow, nullptr);
    DestroyMenu(menu);
    PostMessageW(m_messageWindow, WM_NULL, 0, 0);

    switch (command) {
        case kMenuTrayOpen:
            ShowSettings();
            break;
        case kMenuTrayToggle:
            m_app->ToggleEnabled();
            break;
        case kMenuTrayExit:
            m_app->RequestExit();
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// 设置窗口：窗口过程
// ---------------------------------------------------------------------------

LRESULT CALLBACK TrayUI::SettingsWindowProc(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam) {
    TrayUI* self = reinterpret_cast<TrayUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<TrayUI*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self != nullptr) {
        return self->HandleSettingsWindow(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK TrayUI::CardWindowProc(HWND hwnd, UINT message,
                                        WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);

            RECT rect{};
            GetClientRect(hwnd, &rect);

            const int radius = ui::Scale(hwnd, 8);

            HBRUSH background = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            HPEN   border     = CreatePen(PS_SOLID, 1, RGB(0xE1, 0xE3, 0xE8));

            HGDIOBJ previousBrush = SelectObject(dc, background);
            HGDIOBJ previousPen   = SelectObject(dc, border);

            RoundRect(dc, rect.left, rect.top, rect.right - 1, rect.bottom - 1,
                      radius, radius);

            SelectObject(dc, previousPen);
            SelectObject(dc, previousBrush);
            DeleteObject(border);
            DeleteObject(background);

            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// ---------------------------------------------------------------------------
// 设置窗口：布局
// ---------------------------------------------------------------------------

void TrayUI::LayoutSettings() {
    if (m_settingsWindow == nullptr) {
        return;
    }

    // 先建新字体再替换旧字体，避免出现控件仍引用已释放 HFONT 的瞬间
    HFONT oldTitleFont = m_titleFont;
    HFONT oldBoldFont  = m_boldFont;
    HFONT oldBodyFont  = m_bodyFont;
    HFONT oldSmallFont = m_smallFont;

    m_titleFont = ui::CreateUiFont(m_settingsWindow, 16, FW_SEMIBOLD);
    m_boldFont  = ui::CreateUiFont(m_settingsWindow, 11, FW_SEMIBOLD);
    m_bodyFont  = ui::CreateUiFont(m_settingsWindow, 10, FW_NORMAL);
    m_smallFont = ui::CreateUiFont(m_settingsWindow, 9, FW_NORMAL);

    RECT desired{0, 0, ui::Scale(m_settingsWindow, kClientWidth),
                 ui::Scale(m_settingsWindow, kClientHeight)};
    const DWORD style   = static_cast<DWORD>(GetWindowLongPtrW(m_settingsWindow, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(m_settingsWindow, GWL_EXSTYLE));
    if (AdjustWindowRectEx(&desired, style, FALSE, exStyle)) {
        SetWindowPos(m_settingsWindow, nullptr, 0, 0,
                     desired.right - desired.left,
                     desired.bottom - desired.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    const auto s = [this](int value) { return ui::Scale(m_settingsWindow, value); };
    const int contentWidth = kClientWidth - kMargin * 2;

    // 顶部：软件信息
    MoveWindow(m_titleLabel,   s(kMargin), s(16), s(220), s(28), TRUE);
    MoveWindow(m_versionLabel, s(kMargin + 240), s(22), s(contentWidth - 240), s(20), TRUE);
    MoveWindow(m_statusLabel,  s(kMargin), s(48), s(contentWidth), s(20), TRUE);

    // 中部：侧键配置卡片
    for (int i = 0; i < kButtonCount; ++i) {
        const int cardTop = kCardTop[i];

        MoveWindow(m_card[i], s(kMargin), s(cardTop), s(contentWidth), s(kCardHeight), TRUE);
        MoveWindow(m_buttonTitle[i], s(kMargin + 16), s(cardTop + 12), s(200), s(20), TRUE);
        MoveWindow(m_buttonSummary[i], s(kMargin + 16), s(cardTop + 36), s(220), s(20), TRUE);
        MoveWindow(m_buttonEdit[i],
                   s(kMargin + contentWidth - 16 - kEditButtonW),
                   s(cardTop + (kCardHeight - kEditButtonH) / 2),
                   s(kEditButtonW), s(kEditButtonH), TRUE);
    }

    // 底部：基础设置
    MoveWindow(m_enableCheck,    s(kMargin + 2), s(240), s(300), s(24), TRUE);
    MoveWindow(m_autoStartCheck, s(kMargin + 2), s(272), s(300), s(24), TRUE);
    MoveWindow(m_resetButton,    s(kMargin + 2), s(312), s(120), s(32), TRUE);

    SendMessageW(m_titleLabel,       WM_SETFONT, reinterpret_cast<WPARAM>(m_titleFont), TRUE);
    SendMessageW(m_versionLabel,     WM_SETFONT, reinterpret_cast<WPARAM>(m_smallFont), TRUE);
    SendMessageW(m_statusLabel,      WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    SendMessageW(m_enableCheck,      WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    SendMessageW(m_autoStartCheck,   WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    SendMessageW(m_resetButton,      WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);

    for (int i = 0; i < kButtonCount; ++i) {
        SendMessageW(m_buttonTitle[i],   WM_SETFONT, reinterpret_cast<WPARAM>(m_boldFont), TRUE);
        SendMessageW(m_buttonSummary[i], WM_SETFONT, reinterpret_cast<WPARAM>(m_smallFont), TRUE);
        SendMessageW(m_buttonEdit[i],    WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    }

    // 所有控件都换上新字体后再释放旧字体
    HFONT staleFonts[] = {oldTitleFont, oldBoldFont, oldBodyFont, oldSmallFont};
    for (HFONT font : staleFonts) {
        if (font != nullptr) {
            DeleteObject(font);
        }
    }
}

// ---------------------------------------------------------------------------
// 设置窗口：内容刷新
// ---------------------------------------------------------------------------

void TrayUI::RefreshSettingsContent() {
    if (m_settingsWindow == nullptr || m_app == nullptr) {
        return;
    }

    m_refreshingControls = true;

    const bool enabled = m_app->IsEnabled();

    SetWindowTextW(m_versionLabel, kAppVersion);
    SetWindowTextW(m_statusLabel,
                   enabled ? L"●  侧键映射已启用" : L"●  侧键映射已暂停");

    Button_SetCheck(m_enableCheck, enabled ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(m_autoStartCheck,
                    m_app->IsAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED);

    for (int i = 0; i < kButtonCount; ++i) {
        RefreshButtonRow(i);
    }

    m_refreshingControls = false;

    InvalidateRect(m_statusLabel, nullptr, TRUE);
}

void TrayUI::RefreshButtonRow(int index) {
    if (m_app == nullptr || index < 0 || index >= kButtonCount) {
        return;
    }

    const MappedAction action = m_app->GetButtonAction(index);

    SetWindowTextW(m_buttonTitle[index], ButtonDisplayName(index).c_str());
    SetWindowTextW(m_buttonSummary[index], ActionSummary(action).c_str());

    // 「保留原始功能」和「禁用」都是不拦截的状态
    const bool intercepted = (action.type != ActionType::KeepOriginal &&
                              action.type != ActionType::Disabled);
    SetWindowTextW(m_buttonEdit[index], intercepted ? L"修改" : L"设置");

    InvalidateRect(m_buttonTitle[index], nullptr, TRUE);
    InvalidateRect(m_buttonSummary[index], nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// 设置窗口：消息处理
// ---------------------------------------------------------------------------

LRESULT TrayUI::HandleSettingsWindow(HWND hwnd, UINT message,
                                     WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            if (m_settingsWindow == nullptr) {
                m_settingsWindow = hwnd;
            }
            m_titleLabel = CreateWindowExW(0, L"STATIC", kAppName,
                                           WS_CHILD | WS_VISIBLE | SS_LEFT,
                                           0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            m_versionLabel = CreateWindowExW(0, L"STATIC", kAppVersion,
                                             WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                             0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            m_statusLabel = CreateWindowExW(0, L"STATIC", L"",
                                            WS_CHILD | WS_VISIBLE | SS_LEFT,
                                            0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            for (int i = 0; i < kButtonCount; ++i) {
                m_card[i] = CreateWindowExW(0, kCardWindowClass, L"",
                                            WS_CHILD | WS_VISIBLE | WS_DISABLED,
                                            0, 0, 0, 0, hwnd,
                                            reinterpret_cast<HMENU>(
                                                static_cast<INT_PTR>(kIdCardBase + i)),
                                            nullptr, nullptr);

                m_buttonTitle[i] = CreateWindowExW(0, L"STATIC", L"",
                                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                   0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                m_buttonSummary[i] = CreateWindowExW(0, L"STATIC", L"",
                                                     WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                     0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                m_buttonEdit[i] = CreateWindowExW(0, L"BUTTON", L"修改",
                                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                                  0, 0, 0, 0, hwnd,
                                                  reinterpret_cast<HMENU>(
                                                      static_cast<INT_PTR>(kIdEditButtonBase + i)),
                                                  nullptr, nullptr);
            }

            m_enableCheck = CreateWindowExW(0, L"BUTTON", L"启用侧键映射",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                            0, 0, 0, 0, hwnd,
                                            reinterpret_cast<HMENU>(
                                                static_cast<INT_PTR>(kIdEnableCheck)),
                                            nullptr, nullptr);
            m_autoStartCheck = CreateWindowExW(0, L"BUTTON", L"开机自动启动",
                                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                               0, 0, 0, 0, hwnd,
                                               reinterpret_cast<HMENU>(
                                                   static_cast<INT_PTR>(kIdAutoStartCheck)),
                                               nullptr, nullptr);
            m_resetButton = CreateWindowExW(0, L"BUTTON", L"恢复默认配置",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                            0, 0, 0, 0, hwnd,
                                            reinterpret_cast<HMENU>(
                                                static_cast<INT_PTR>(kIdResetButton)),
                                            nullptr, nullptr);

            LayoutSettings();
            RefreshSettingsContent();
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC  dc      = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);

            SetBkMode(dc, TRANSPARENT);

            bool onCard = false;
            if (control == m_statusLabel) {
                SetTextColor(dc, (m_app != nullptr && m_app->IsEnabled())
                                     ? RGB(0x1A, 0x53, 0xD8)
                                     : RGB(0x8A, 0x8F, 0x98));
            } else if (control == m_versionLabel ||
                       control == m_buttonSummary[0] ||
                       control == m_buttonSummary[1]) {
                SetTextColor(dc, RGB(0x8A, 0x8F, 0x98));
            } else {
                SetTextColor(dc, RGB(0x1F, 0x23, 0x28));
            }

            onCard = (control == m_buttonTitle[0] || control == m_buttonTitle[1] ||
                      control == m_buttonSummary[0] || control == m_buttonSummary[1]);

            return reinterpret_cast<LRESULT>(onCard ? m_cardBrush : m_windowBrush);
        }

        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(0x1F, 0x23, 0x28));
            return reinterpret_cast<LRESULT>(m_windowBrush);
        }

        case WM_ERASEBKGND: {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, m_windowBrush);
            return 1;
        }

        case WM_COMMAND: {
            const int controlId = LOWORD(wParam);

            if (controlId >= kIdEditButtonBase &&
                controlId < kIdEditButtonBase + kButtonCount) {
                ShowActionMenu(controlId - kIdEditButtonBase);
                return 0;
            }

            if (controlId == kIdEnableCheck) {
                if (!m_refreshingControls && m_app != nullptr) {
                    const bool want = Button_GetCheck(m_enableCheck) == BST_CHECKED;
                    m_app->SetEnabled(want);
                }
                return 0;
            }

            if (controlId == kIdAutoStartCheck) {
                if (!m_refreshingControls && m_app != nullptr) {
                    const bool want = Button_GetCheck(m_autoStartCheck) == BST_CHECKED;
                    m_app->SetAutoStart(want);
                }
                return 0;
            }

            if (controlId == kIdResetButton) {
                if (m_app != nullptr) {
                    m_app->ResetToDefaults();
                }
                return 0;
            }

            // Esc：由 IsDialogMessage 转发的 IDCANCEL，等同于关闭窗口
            if (controlId == IDCANCEL && lParam == 0) {
                HideSettings();
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            HideSettings();  // 关闭窗口不等于退出程序
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
            LayoutSettings();
            return 0;
        }

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0u) == SC_KEYMENU) {
                return 0;  // 屏蔽 Alt 菜单激活的提示音
            }
            break;

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// ---------------------------------------------------------------------------
// 侧键动作选择菜单
// ---------------------------------------------------------------------------

void TrayUI::ShowActionMenu(int buttonIndex) {
    if (m_app == nullptr || buttonIndex < 0 || buttonIndex >= kButtonCount) {
        return;
    }

    const MappedAction current = m_app->GetButtonAction(buttonIndex);

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    UINT commandId = kMenuPresetBase;
    for (ActionType type : kPresetMenuActions) {
        const std::wstring text = PresetMenuText(type);
        AppendMenuW(menu, MenuFlags(current.type == type), commandId, text.c_str());
        ++commandId;
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    std::wstring customText = L"自定义快捷键…";
    if (current.type == ActionType::Custom && current.vk != 0) {
        customText += L"\t";
        customText += HotkeyToString(current.mods, current.vk);
    }
    AppendMenuW(menu, MenuFlags(current.type == ActionType::Custom),
                kMenuCustom, customText.c_str());

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MenuFlags(current.type == ActionType::KeepOriginal),
                kMenuKeepOriginal, L"保留鼠标原始功能");
    AppendMenuW(menu, MenuFlags(current.type == ActionType::Disabled),
                kMenuDisabled, L"禁用此侧键");

    // 以「修改」按钮为锚点弹出
    RECT anchor{};
    if (m_buttonEdit[buttonIndex] == nullptr ||
        !GetWindowRect(m_buttonEdit[buttonIndex], &anchor)) {
        if (!GetWindowRect(m_settingsWindow, &anchor)) {
            DestroyMenu(menu);
            return;
        }
    }

    SetForegroundWindow(m_settingsWindow);
    const UINT command = TrackPopupMenu(menu,
                                        TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_NONOTIFY,
                                        anchor.left, anchor.bottom + ui::Scale(m_settingsWindow, 4),
                                        0, m_settingsWindow, nullptr);
    DestroyMenu(menu);
    PostMessageW(m_settingsWindow, WM_NULL, 0, 0);

    if (command == 0) {
        return;  // 用户取消
    }

    if (command >= kMenuPresetBase &&
        command < kMenuPresetBase + ARRAYSIZE(kPresetMenuActions)) {
        const size_t index = command - kMenuPresetBase;
        MappedAction action;
        action.type = kPresetMenuActions[index];
        ApplyAction(buttonIndex, action);
        return;
    }

    switch (command) {
        case kMenuCustom:
            BeginRecording(buttonIndex);
            break;
        case kMenuKeepOriginal: {
            MappedAction action;
            action.type = ActionType::KeepOriginal;
            ApplyAction(buttonIndex, action);
            break;
        }
        case kMenuDisabled: {
            MappedAction action;
            action.type = ActionType::Disabled;
            ApplyAction(buttonIndex, action);
            break;
        }
        default:
            break;
    }
}

void TrayUI::ApplyAction(int buttonIndex, const MappedAction& action) {
    if (m_app == nullptr) {
        return;
    }
    // App 内部会立即更新钩子拦截状态并落盘配置
    m_app->SetButtonAction(buttonIndex, action);
    RefreshButtonRow(buttonIndex);
}

// ---------------------------------------------------------------------------
// 快捷键录制
// ---------------------------------------------------------------------------

void TrayUI::BeginRecording(int buttonIndex) {
    if (m_app == nullptr || buttonIndex < 0 || buttonIndex >= kButtonCount) {
        return;
    }
    if (m_recorder.IsOpen()) {
        return;
    }

    const MappedAction current = m_app->GetButtonAction(buttonIndex);
    const UINT mods = (current.type == ActionType::Custom) ? current.mods : MOD_NONE;
    const UINT vk   = (current.type == ActionType::Custom) ? current.vk : 0;

    m_recorder.Open(m_settingsWindow, mods, vk,
                    [this, buttonIndex](bool confirmed, UINT newMods, UINT newVk) {
                        if (!confirmed) {
                            return;
                        }
                        MappedAction action;
                        action.type = ActionType::Custom;
                        action.mods = newMods & (MOD_CTRL | MOD_ALT | MOD_SHIFT | MOD_WIN);
                        action.vk   = newVk;
                        ApplyAction(buttonIndex, action);
                    });
}

// ---------------------------------------------------------------------------
// 显示 / 隐藏 / 状态同步
// ---------------------------------------------------------------------------

void TrayUI::ShowSettings() {
    if (m_settingsWindow == nullptr) {
        return;
    }

    RefreshSettingsContent();

    if (IsIconic(m_settingsWindow)) {
        ShowWindow(m_settingsWindow, SW_RESTORE);
    } else {
        ShowWindow(m_settingsWindow, SW_SHOWNORMAL);
    }
    SetForegroundWindow(m_settingsWindow);
}

void TrayUI::HideSettings() {
    if (m_settingsWindow == nullptr) {
        return;
    }
    ShowWindow(m_settingsWindow, SW_HIDE);
}

bool TrayUI::PreTranslateMessage(MSG& message) {
    if (m_settingsWindow != nullptr &&
        IsWindowVisible(m_settingsWindow) &&
        IsDialogMessageW(m_settingsWindow, &message)) {
        return true;
    }
    return false;
}

void TrayUI::RefreshStatus() {
    RefreshTrayIcon();

    if (m_settingsWindow == nullptr) {
        return;
    }
    if (IsWindowVisible(m_settingsWindow)) {
        RefreshSettingsContent();
    }
}

void TrayUI::ShowError(const std::wstring& message) {
    MessageBoxW(m_settingsWindow != nullptr ? m_settingsWindow : m_messageWindow,
                message.c_str(),
                kAppName,
                MB_OK | MB_ICONERROR);
}

}  // namespace ms
