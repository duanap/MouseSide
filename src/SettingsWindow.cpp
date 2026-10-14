#include "SettingsWindow.h"

#include "Config.h"
#include "KeyMapper.h"
#include "resource.h"

#include <commctrl.h>
#include <windowsx.h>

#include <iterator>
#include <algorithm>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

namespace ms {
namespace settings {

namespace {

enum CtrlId : int {
    kIdTitle = 200,
    kIdVersion,
    kIdStatus,
    kIdGroup4, kIdLabel4Action, kIdCombo4, kIdLabel4Key, kIdEdit4, kIdBtn4Record,
    kIdGroup5, kIdLabel5Action, kIdCombo5, kIdLabel5Key, kIdEdit5, kIdBtn5Record,
    kIdChkEnable, kIdChkAutoStart, kIdBtnReset, kIdHint, kIdBtnExit,
};

struct LayoutItem {
    int id;
    int x, y, w, h;   // 96 DPI 下的逻辑坐标
    int point;        // 字号（磅）
    bool bold;
};

// 组合框下拉区域高度单独指定（折叠显示高度由字体决定）
constexpr int kComboDropHeight = 200;
constexpr int kClientW = 384;
constexpr int kClientH = 470;

constexpr LayoutItem kLayout[] = {
    {kIdTitle,        20,  18, 260, 26, 17, true },
    {kIdVersion,      20,  48,  96, 18,  9, false},
    {kIdStatus,      120,  48, 240, 18,  9, false},
    {kIdGroup4,       16,  74, 352, 114, 9, false},
    {kIdLabel4Action, 30, 102,  56, 20,  9, false},
    {kIdCombo4,       92,  98, 260, kComboDropHeight, 9, false},
    {kIdLabel4Key,    30, 134,  56, 20,  9, false},
    {kIdEdit4,        92, 132, 196, 24,  9, false},
    {kIdBtn4Record,  296, 131,  56, 26,  9, false},
    {kIdGroup5,       16, 200, 352, 114, 9, false},
    {kIdLabel5Action, 30, 228,  56, 20,  9, false},
    {kIdCombo5,       92, 224, 260, kComboDropHeight, 9, false},
    {kIdLabel5Key,    30, 260,  56, 20,  9, false},
    {kIdEdit5,        92, 258, 196, 24,  9, false},
    {kIdBtn5Record,  296, 257,  56, 26,  9, false},
    {kIdChkEnable,    20, 328, 220, 22,  9, false},
    {kIdChkAutoStart, 20, 354, 220, 22,  9, false},
    {kIdBtnReset,     20, 388, 132, 28,  9, false},
    {kIdBtnExit,     232, 388, 132, 28,  9, false},
    {kIdHint,         20, 424, 344, 36,  8, false},
};

HINSTANCE g_hInst = nullptr;
HWND g_hwnd = nullptr;
HFONT g_font = nullptr;
HFONT g_fontBold = nullptr;
HFONT g_fontSmall = nullptr;
HBRUSH g_bgBrush = nullptr;
UINT g_dpi = 96;

// ---- 快捷键录制 ----
struct CaptureState {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    HHOOK hook = nullptr;
    Hotkey hotkey{};
    bool active = false;
    bool down[256]{};
    bool initiallyDown[256]{};
    bool swallowed[256]{};
    bool ending = false;
    bool success = false;
    bool finishQueued = false;
    int targetCombo = 0;  // 录制结果写入哪个侧键（kIdCombo4 / kIdCombo5）
};
CaptureState g_capture{};

struct CaptureResult {
    int targetCombo;
    Hotkey hotkey;
};

bool KeyDownNow(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

HFONT MakeFont(int point, bool bold, UINT dpi) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(point, static_cast<int>(dpi), 72);
    lf.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    // 避免依赖 CRT 的 _s 系列函数，直接按长度拷贝
    const wchar_t* face = L"Segoe UI";
    size_t i = 0;
    for (; i + 1 < std::size(lf.lfFaceName) && face[i]; ++i) lf.lfFaceName[i] = face[i];
    lf.lfFaceName[i] = L'\0';
    return CreateFontIndirectW(&lf);
}

void CreateFonts(UINT dpi) {
    if (g_font) DeleteObject(g_font);
    if (g_fontBold) DeleteObject(g_fontBold);
    if (g_fontSmall) DeleteObject(g_fontSmall);
    g_font = MakeFont(9, false, dpi);
    g_fontBold = MakeFont(17, true, dpi);
    g_fontSmall = MakeFont(8, false, dpi);
}

void ApplyFonts() {
    for (const auto& item : kLayout) {
        HWND h = GetDlgItem(g_hwnd, item.id);
        if (!h) continue;
        HFONT font = item.bold ? g_fontBold : (item.point <= 8 ? g_fontSmall : g_font);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

// 96 DPI 逻辑坐标 -> 当前 DPI 物理像素
int Sx(int value) {
    return MulDiv(value, static_cast<int>(g_dpi), 96);
}

bool IsCombo(int id) {
    return id == kIdCombo4 || id == kIdCombo5;
}

const LayoutItem* FindLayout(int id) {
    for (const auto& item : kLayout) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

// updateComboHeight：仅 DPI 变化时更新下拉列表高度；平时保持组合框既有高度
void Relayout(bool updateComboHeight) {
    for (const auto& item : kLayout) {
        HWND h = GetDlgItem(g_hwnd, item.id);
        if (!h) continue;
        int height = Sx(item.h);
        if (IsCombo(item.id) && !updateComboHeight) {
            RECT rect{};
            GetWindowRect(h, &rect);
            height = rect.bottom - rect.top;
        }
        MoveWindow(h, Sx(item.x), Sx(item.y), Sx(item.w), height, TRUE);
    }
}

void ResizeWindowForDpi() {
    const double scale = static_cast<double>(g_dpi) / 96.0;
    RECT rect{0, 0, static_cast<LONG>(kClientW * scale), static_cast<LONG>(kClientH * scale)};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(g_hwnd, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE));

    using AdjustFn = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto adjust = reinterpret_cast<AdjustFn>(GetProcAddress(user32, "AdjustWindowRectExForDpi"));
    if (adjust) {
        adjust(&rect, style, FALSE, exStyle, g_dpi);
    } else {
        AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    }
    SetWindowPos(g_hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---- 组合框 ----
constexpr int kIndexCustom = 8;
constexpr int kIndexOriginal = 9;
constexpr int kIndexDisabled = 10;

void FillCombo(HWND combo) {
    for (const PresetDef* p = PresetTable(); p && p->label; ++p) {
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(keymap::PresetDisplayFull(p->id).c_str()));
    }
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"自定义快捷键…"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"保留鼠标原始功能"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"禁用此侧键"));
}

int ComboIndexForMapping(const Mapping& m) {
    switch (m.kind) {
        case ActionKind::Preset: {
            int index = 0;
            for (const PresetDef* p = PresetTable(); p && p->label; ++p, ++index) {
                if (p->id == m.preset) return index;
            }
            return 0;
        }
        case ActionKind::Custom:   return kIndexCustom;
        case ActionKind::Original: return kIndexOriginal;
        case ActionKind::Disabled: break;
    }
    return kIndexDisabled;
}

std::wstring HotkeyDisplayText(const Mapping& m) {
    switch (m.kind) {
        case ActionKind::Preset:   return keymap::HotkeyText(PresetHotkey(m.preset));
        case ActionKind::Custom:   return keymap::HotkeyText(m.hotkey);
        case ActionKind::Original: return L"（不拦截侧键）";
        case ActionKind::Disabled: break;
    }
    return L"（无动作）";
}

void RefreshButton(int comboId, int editId, const Mapping& m) {
    HWND combo = GetDlgItem(g_hwnd, comboId);
    HWND edit = GetDlgItem(g_hwnd, editId);
    if (combo) SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(ComboIndexForMapping(m)), 0);
    if (edit) SetWindowTextW(edit, HotkeyDisplayText(m).c_str());
}

// ---- 快捷键录制窗口 ----
void CaptureFinish(bool ok);
void CaptureUpdateDisplay();
void CaptureRequestCancel();
constexpr UINT kCaptureUpdate = WM_APP + 20;
constexpr UINT kCaptureFinish = WM_APP + 21;
constexpr UINT kCaptureCancel = WM_APP + 22;

bool CaptureDrained() {
    return std::none_of(std::begin(g_capture.swallowed), std::end(g_capture.swallowed),
                        [](bool value) { return value; });
}

void QueueCaptureFinish() {
    if (g_capture.ending && !g_capture.finishQueued && CaptureDrained()) {
        g_capture.finishQueued = true;
        PostMessageW(g_capture.hwnd, kCaptureFinish, 0, 0);
    }
}

Hotkey CaptureModifiers() {
    const auto* down = g_capture.down;
    return Hotkey{0,
        down[VK_CONTROL] || down[VK_LCONTROL] || down[VK_RCONTROL],
        down[VK_MENU] || down[VK_LMENU] || down[VK_RMENU],
        down[VK_SHIFT] || down[VK_LSHIFT] || down[VK_RSHIFT],
        down[VK_LWIN] || down[VK_RWIN]};
}

LRESULT CALLBACK CaptureProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case kCaptureUpdate:
            CaptureUpdateDisplay();
            if (!g_capture.ending || g_capture.success) SetTimer(hwnd, 1, 15000, nullptr);
            return 0;
        case kCaptureFinish:
            g_capture.finishQueued = false;
            if (CaptureDrained()) CaptureFinish(g_capture.success);
            return 0;
        case kCaptureCancel:
            CaptureRequestCancel();
            return 0;
        case WM_CREATE: {
            HWND tip = CreateWindowW(L"STATIC", L"请按下要使用的快捷键组合…",
                                     WS_CHILD | WS_VISIBLE | SS_CENTER,
                                     0, 0, 0, 0, hwnd, nullptr, g_hInst, nullptr);
            HWND display = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                         0, 0, 0, 0, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<UINT_PTR>(2)),
                                         g_hInst, nullptr);
            HWND hint = CreateWindowW(L"STATIC", L"Esc 取消，15 秒无操作自动取消",
                                      WS_CHILD | WS_VISIBLE | SS_CENTER,
                                      0, 0, 0, 0, hwnd, nullptr, g_hInst, nullptr);
            HFONT regular = MakeFont(9, false, g_dpi);
            HFONT big = MakeFont(14, true, g_dpi);
            SendMessageW(tip, WM_SETFONT, reinterpret_cast<WPARAM>(regular), TRUE);
            SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(regular), TRUE);
            SendMessageW(display, WM_SETFONT, reinterpret_cast<WPARAM>(big), TRUE);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(regular));
            SetWindowLongPtrW(hwnd, 0, reinterpret_cast<LONG_PTR>(big));

            const int w = MulDiv(340, static_cast<int>(g_dpi), 96);
            MoveWindow(tip, 0, MulDiv(24, static_cast<int>(g_dpi), 96), w,
                       MulDiv(20, static_cast<int>(g_dpi), 96), TRUE);
            MoveWindow(display, 0, MulDiv(58, static_cast<int>(g_dpi), 96), w,
                       MulDiv(34, static_cast<int>(g_dpi), 96), TRUE);
            MoveWindow(hint, 0, MulDiv(112, static_cast<int>(g_dpi), 96), w,
                       MulDiv(20, static_cast<int>(g_dpi), 96), TRUE);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, RGB(255, 255, 255));
            SetTextColor(dc, RGB(26, 26, 26));
            return reinterpret_cast<LRESULT>(
                g_bgBrush ? g_bgBrush : reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        }
        case WM_ERASEBKGND: {
            HDC dc = reinterpret_cast<HDC>(wp);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            FillRect(dc, &rect,
                     g_bgBrush ? g_bgBrush : reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            return 1;
        }
        case WM_TIMER:
            if (wp == 1) {
                KillTimer(hwnd, 1);
                CaptureRequestCancel();
            }
            return 0;
        case WM_DESTROY: {
            HFONT regular = reinterpret_cast<HFONT>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            HFONT big = reinterpret_cast<HFONT>(GetWindowLongPtrW(hwnd, 0));
            if (regular) DeleteObject(regular);
            if (big) DeleteObject(big);
            return 0;
        }
        case WM_CLOSE:
            CaptureRequestCancel();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void CaptureUpdateDisplay() {
    if (!g_capture.hwnd) return;
    HWND display = GetDlgItem(g_capture.hwnd, 2);
    if (!display) return;
    if (g_capture.hotkey.IsEmpty()) {
        std::wstring text;
        const Hotkey mods = CaptureModifiers();
        if (mods.ctrl) text += L"Ctrl + ";
        if (mods.alt) text += L"Alt + ";
        if (mods.shift) text += L"Shift + ";
        if (mods.win) text += L"Win + ";
        text += L"…";
        SetWindowTextW(display, text.c_str());
    } else {
        SetWindowTextW(display, keymap::HotkeyText(g_capture.hotkey).c_str());
    }
}

LRESULT CALLBACK CaptureKeyProc(int nCode, WPARAM wp, LPARAM lp) {
    if (nCode == HC_ACTION && g_capture.active) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        if ((info->flags & LLKHF_INJECTED) || info->vkCode > 255)
            return CallNextHookEx(g_capture.hook, nCode, wp, lp);
        const bool isUp = (wp == WM_KEYUP || wp == WM_SYSKEYUP);
        const unsigned vk = info->vkCode;
        if (g_capture.ending && !g_capture.success) {
            // Cancelled recording only drains keys it already swallowed.
            // New keyboard input passes through even if a held key never releases.
            const bool block = g_capture.swallowed[vk];
            if (isUp && block) {
                g_capture.swallowed[vk] = false;
                g_capture.down[vk] = false;
                QueueCaptureFinish();
            }
            return block ? 1 : CallNextHookEx(g_capture.hook, nCode, wp, lp);
        }
        const bool passthrough = g_capture.initiallyDown[vk];
        g_capture.down[vk] = !isUp;
        if (isUp) {
            g_capture.initiallyDown[vk] = false;
            g_capture.swallowed[vk] = false;
        } else if (!passthrough) {
            g_capture.swallowed[vk] = true;
        }
        if (!isUp && vk == VK_ESCAPE) {
            g_capture.ending = true;
            g_capture.success = false;
            PostMessageW(g_capture.hwnd, kCaptureCancel, 0, 0);
        } else if (!g_capture.ending && !isUp && !passthrough) {
            if (!IsModifierKey(vk)) {
                g_capture.hotkey = CaptureModifiers();
                g_capture.hotkey.vk = vk;
                g_capture.ending = true;
                g_capture.success = true;
            }
        }
        // Do no UI/resource work in this hook. Drain swallowed key releases
        // before uninstalling it so captured Win/Alt combinations cannot leak.
        PostMessageW(g_capture.hwnd, kCaptureUpdate, 0, 0);
        QueueCaptureFinish();
        if (!passthrough) return 1;
    }
    return CallNextHookEx(g_capture.hook, nCode, wp, lp);
}

void CaptureFinish(bool ok) {
    if (!g_capture.active) return;
    const CaptureResult result{g_capture.targetCombo, g_capture.hotkey};
    g_capture.active = false;
    if (g_capture.hook) {
        UnhookWindowsHookEx(g_capture.hook);
        g_capture.hook = nullptr;
    }
    if (g_capture.hwnd) {
        KillTimer(g_capture.hwnd, 1);
        DestroyWindow(g_capture.hwnd);
        g_capture.hwnd = nullptr;
    }
    HWND owner = g_capture.owner;
    g_capture.owner = nullptr;
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
        // Deliver a snapshot synchronously; a subsequent recording must not
        // replace the result or inherit a queued cancellation.
        SendMessageW(owner, WM_MS_CAPTURE_RESULT, ok ? 1 : 0,
                     reinterpret_cast<LPARAM>(&result));
    }
}

void CaptureRequestCancel() {
    if (!g_capture.active) return;
    g_capture.ending = true;
    g_capture.success = false;
    KillTimer(g_capture.hwnd, 1);
    if (CaptureDrained()) {
        CaptureFinish(false);
        return;
    }
    ShowWindow(g_capture.hwnd, SW_HIDE);
    if (g_capture.owner) {
        EnableWindow(g_capture.owner, TRUE);
        SetForegroundWindow(g_capture.owner);
        RefreshFromConfig();
    }
}

void CaptureStart(HWND owner, int targetCombo) {
    if (g_capture.active) return;

    g_capture = CaptureState{};
    g_capture.owner = owner;
    g_capture.targetCombo = targetCombo;
    g_capture.hotkey = Hotkey{};
    g_capture.active = true;
    for (unsigned vk = 1; vk < 256; ++vk) {
        // Generic modifier aliases are not distinct physical keys. LL events
        // report the sided key, so avoid a stale second copy in the snapshot.
        if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT) continue;
        g_capture.down[vk] = KeyDownNow(static_cast<int>(vk));
        g_capture.initiallyDown[vk] = g_capture.down[vk];
    }

    g_capture.hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kCaptureClass, L"录制快捷键",
                                     WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                     CW_USEDEFAULT, CW_USEDEFAULT, 340, 170,
                                     GetWindow(owner, GW_OWNER) ? GetWindow(owner, GW_OWNER) : owner,
                                     nullptr, g_hInst, nullptr);
    if (!g_capture.hwnd) {
        g_capture.active = false;
        return;
    }

    const int w = MulDiv(340, static_cast<int>(g_dpi), 96);
    const int h = MulDiv(170, static_cast<int>(g_dpi), 96);
    RECT rect{0, 0, w, h};
    AdjustWindowRectEx(&rect, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    SetWindowPos(g_capture.hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER);

    // 居中于设置窗口
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - (rect.right - rect.left)) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - (rect.bottom - rect.top)) / 2;
    SetWindowPos(g_capture.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    g_capture.hook = SetWindowsHookExW(WH_KEYBOARD_LL, CaptureKeyProc, nullptr, 0);
    if (!g_capture.hook) {
        MessageBoxW(owner, L"无法启动键盘监听，快捷键录制失败。", kAppName, MB_ICONWARNING | MB_OK);
        CaptureFinish(false);
        return;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(g_capture.hwnd, SW_SHOW);
    SetTimer(g_capture.hwnd, 1, 15000, nullptr);
    CaptureUpdateDisplay();
}

// ---- 设置窗口 ----
void CreateControls() {
    const auto create = [](const wchar_t* className, const wchar_t* text, int id, DWORD style) {
        return CreateWindowW(className, text, WS_CHILD | WS_VISIBLE | style,
                             0, 0, 0, 0, g_hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                             g_hInst, nullptr);
    };

    create(L"STATIC", kAppName, kIdTitle, 0);
    create(L"STATIC", kVersionText, kIdVersion, 0);
    create(L"STATIC", L"", kIdStatus, 0);

    create(L"BUTTON", L"鼠标第四按钮 [4]", kIdGroup4, BS_GROUPBOX);
    create(L"STATIC", L"功能", kIdLabel4Action, 0);
    HWND combo4 = create(WC_COMBOBOXW, L"", kIdCombo4, CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL);
    create(L"STATIC", L"快捷键", kIdLabel4Key, 0);
    create(WC_EDITW, L"", kIdEdit4, ES_READONLY | ES_AUTOHSCROLL);
    create(L"BUTTON", L"录制", kIdBtn4Record, BS_PUSHBUTTON);

    create(L"BUTTON", L"鼠标第五按钮 [5]", kIdGroup5, BS_GROUPBOX);
    create(L"STATIC", L"功能", kIdLabel5Action, 0);
    HWND combo5 = create(WC_COMBOBOXW, L"", kIdCombo5, CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL);
    create(L"STATIC", L"快捷键", kIdLabel5Key, 0);
    create(WC_EDITW, L"", kIdEdit5, ES_READONLY | ES_AUTOHSCROLL);
    create(L"BUTTON", L"录制", kIdBtn5Record, BS_PUSHBUTTON);

    create(L"BUTTON", L"启用侧键映射", kIdChkEnable, BS_AUTOCHECKBOX);
    create(L"BUTTON", L"开机自动启动", kIdChkAutoStart, BS_AUTOCHECKBOX);
    create(L"BUTTON", L"恢复默认配置", kIdBtnReset, BS_PUSHBUTTON);
    create(L"BUTTON", L"退出程序", kIdBtnExit, BS_PUSHBUTTON);
    create(L"STATIC", L"关闭窗口后继续后台运行；可通过托盘菜单或“退出程序”按钮完全退出。",
           kIdHint, 0);

    FillCombo(combo4);
    FillCombo(combo5);

    // 组合框在创建时指定下拉列表高度（含展开区域），之后 Relayout 不再改动其高度
    const auto placeCombo = [](HWND combo, int id) {
        const LayoutItem* item = FindLayout(id);
        if (!combo || !item) return;
        SetWindowPos(combo, nullptr, Sx(item->x), Sx(item->y), Sx(item->w), Sx(kComboDropHeight),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    };
    placeCombo(combo4, kIdCombo4);
    placeCombo(combo5, kIdCombo5);
}

Mapping& MappingForButton(int comboId) {
    AppConfig& cfg = GetConfig();
    return (comboId == kIdCombo4) ? cfg.button4 : cfg.button5;
}

void OnComboChanged(int comboId) {
    HWND combo = GetDlgItem(g_hwnd, comboId);
    const int index = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (index < 0) return;

    if (index == kIndexCustom) {
        CaptureStart(g_hwnd, comboId);
        return;
    }

    Mapping& target = MappingForButton(comboId);
    if (index == kIndexOriginal) {
        target = Mapping{ActionKind::Original, PresetAction::Copy, {}};
    } else if (index == kIndexDisabled) {
        target = Mapping{ActionKind::Disabled, PresetAction::Copy, {}};
    } else {
        int i = 0;
        for (const PresetDef* p = PresetTable(); p && p->label; ++p, ++i) {
            if (i == index) {
                target = Mapping{ActionKind::Preset, p->id, {}};
                break;
            }
        }
    }
    ApplyConfig();
}

void OnCaptureResult(WPARAM ok, LPARAM lp) {
    const auto* result = reinterpret_cast<const CaptureResult*>(lp);
    if (ok && result && (result->targetCombo == kIdCombo4 || result->targetCombo == kIdCombo5) &&
        IsShortcutMainKey(result->hotkey.vk)) {
        Mapping& target = MappingForButton(result->targetCombo);
        target = Mapping{ActionKind::Custom, PresetAction::Copy, result->hotkey};
        ApplyConfig();
    } else {
        RefreshFromConfig();  // 取消：回到录制前的配置显示
    }
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_hwnd = hwnd;
            g_dpi = 96;
            {
                using DpiFn = UINT(WINAPI*)(HWND);
                HMODULE user32 = GetModuleHandleW(L"user32.dll");
                auto dpiFn = reinterpret_cast<DpiFn>(GetProcAddress(user32, "GetDpiForWindow"));
                if (dpiFn) g_dpi = dpiFn(hwnd);
            }
            if (!g_bgBrush) g_bgBrush = CreateSolidBrush(RGB(255, 255, 255));
            CreateControls();
            CreateFonts(g_dpi);
            ApplyFonts();
            ResizeWindowForDpi();
            Relayout(false);
            RefreshFromConfig();
            return 0;

        case WM_ERASEBKGND: {
            HDC dc = reinterpret_cast<HDC>(wp);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            FillRect(dc, &rect, g_bgBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, RGB(255, 255, 255));
            SetTextColor(dc, RGB(26, 26, 26));
            return reinterpret_cast<LRESULT>(g_bgBrush);
        }
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (code == CBN_SELCHANGE && (id == kIdCombo4 || id == kIdCombo5)) {
                OnComboChanged(id);
                return 0;
            }
            if (code == BN_CLICKED) {
                if (id == kIdBtn4Record) CaptureStart(hwnd, kIdCombo4);
                else if (id == kIdBtn5Record) CaptureStart(hwnd, kIdCombo5);
                else if (id == kIdChkEnable) {
                    GetConfig().enabled = (SendMessageW(GetDlgItem(hwnd, id), BM_GETCHECK, 0, 0) == BST_CHECKED);
                    ApplyConfig();
                } else if (id == kIdChkAutoStart) {
                    AppConfig& cfg = GetConfig();
                    const bool want = (SendMessageW(GetDlgItem(hwnd, id), BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (config::SetAutoStart(want)) {
                        cfg.autoStart = want;
                        ApplyConfig();
                    } else {
                        MessageBoxW(hwnd, L"修改开机自动启动失败，可能是注册表权限不足。",
                                    kAppName, MB_ICONWARNING | MB_OK);
                    }
                    RefreshFromConfig();
                } else if (id == kIdBtnReset) {
                    AppConfig& cfg = GetConfig();
                    const bool autoStart = cfg.autoStart;
                    config::ResetToDefault(cfg);
                    cfg.autoStart = autoStart;
                    ApplyConfig();
                } else if (id == kIdBtnExit) {
                    PostMessageW(GetWindow(hwnd, GW_OWNER), WM_COMMAND, kMenuExit, 0);
                }
                return 0;
            }
            break;
        }
        case WM_MS_STATE_CHANGED:
            RefreshFromConfig();
            return 0;
        case WM_MS_CAPTURE_RESULT:
            OnCaptureResult(wp, lp);
            return 0;
        case WM_DPICHANGED: {
            g_dpi = static_cast<UINT>(HIWORD(wp));
            if (const auto* rect = reinterpret_cast<const RECT*>(lp)) {
                SetWindowPos(hwnd, nullptr, rect->left, rect->top,
                             rect->right - rect->left, rect->bottom - rect->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            CreateFonts(g_dpi);
            ApplyFonts();
            Relayout(true);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case WM_DESTROY:
            if (g_capture.active) {
                g_capture.owner = nullptr;
                CaptureRequestCancel();
            }
            g_hwnd = nullptr;
            if (g_font) { DeleteObject(g_font); g_font = nullptr; }
            if (g_fontBold) { DeleteObject(g_fontBold); g_fontBold = nullptr; }
            if (g_fontSmall) { DeleteObject(g_fontSmall); g_fontSmall = nullptr; }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool Register(HINSTANCE hInst) {
    g_hInst = hInst;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kSettingsClass;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    if (!RegisterClassExW(&wc)) return false;

    WNDCLASSEXW cap{};
    cap.cbSize = sizeof(cap);
    cap.lpfnWndProc = CaptureProc;
    cap.hInstance = hInst;
    cap.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cap.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    cap.lpszClassName = kCaptureClass;
    cap.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    cap.cbWndExtra = sizeof(LONG_PTR);  // 存放录制窗口的大号字体句柄
    if (!RegisterClassExW(&cap)) return false;

    return true;
}

void Show(HWND owner) {
    if (g_hwnd) {
        if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
        SetForegroundWindow(g_hwnd);
        return;
    }

    const HWND hwnd = CreateWindowExW(0, kSettingsClass, kAppName,
                                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                      CW_USEDEFAULT, CW_USEDEFAULT, kClientW, kClientH,
                                      owner, nullptr, g_hInst, nullptr);
    if (!hwnd) return;

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

void RefreshFromConfig() {
    if (!g_hwnd) return;
    const AppConfig& cfg = GetConfig();

    RefreshButton(kIdCombo4, kIdEdit4, cfg.button4);
    RefreshButton(kIdCombo5, kIdEdit5, cfg.button5);

    SendMessageW(GetDlgItem(g_hwnd, kIdChkEnable), BM_SETCHECK,
                 cfg.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(GetDlgItem(g_hwnd, kIdChkAutoStart), BM_SETCHECK,
                 cfg.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);

    std::wstring status = cfg.enabled ? L"● 映射已启用" : L"○ 映射已暂停";
    SetWindowTextW(GetDlgItem(g_hwnd, kIdStatus), status.c_str());
}

void CloseAll() {
    if (g_capture.active) CaptureFinish(false);
    if (g_hwnd) DestroyWindow(g_hwnd);
    if (g_bgBrush) { DeleteObject(g_bgBrush); g_bgBrush = nullptr; }
}

}  // namespace settings
}  // namespace ms
