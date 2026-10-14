#include "ui.h"

#include <windows.h>
#include <commctrl.h>
#include <vector>

#include "appfuncs.h"
#include "appstate.h"
#include "mapper.h"
#include "tray.h"

namespace {

constexpr wchar_t kUIClass[] = L"MouseSideUI";
constexpr wchar_t kDlgClass[] = L"MouseSideMapDlg";

constexpr int IDC_BTN_EDIT4 = 2001;
constexpr int IDC_BTN_EDIT5 = 2002;
constexpr int IDC_CHK_ENABLED = 2003;
constexpr int IDC_CHK_AUTO = 2004;
constexpr int IDC_BTN_RESET = 2005;

constexpr int ID_RDO_PRESET = 3001;
constexpr int ID_CMB_PRESET = 3002;
constexpr int ID_RDO_KEYS = 3003;
constexpr int ID_REC = 3004;
constexpr int ID_BTN_CLEAR = 3005;
constexpr int ID_RDO_PASS = 3006;
constexpr int ID_RDO_DIS = 3007;
constexpr int ID_HINT = 3008;

int g_dpi = 96;
HFONT g_font = nullptr;
HFONT g_fontTitle = nullptr;
HWND g_btnEdit4 = nullptr, g_btnEdit5 = nullptr;
HWND g_chkEnabled = nullptr, g_chkAuto = nullptr, g_btnReset = nullptr;

int S(int v) { return MulDiv(v, g_dpi, 96); }

const ButtonConfig& BtnRef(bool isBtn4) {
    return isBtn4 ? app::g_cfg.btn4 : app::g_cfg.btn5;
}
ButtonConfig& BtnRefMut(bool isBtn4) {
    return isBtn4 ? app::g_cfg.btn4 : app::g_cfg.btn5;
}

std::wstring MappingDesc(const ButtonConfig& b) {
    switch (b.type) {
        case ActionType::Preset:
            return mapper::PresetDisplayName(b.preset);
        case ActionType::Keys:
            if (!b.hotkey.valid()) return L"自定义快捷键（未设置）";
            return L"自定义快捷键 · " + HotkeyToText(b.hotkey);
        case ActionType::Passthrough:
            return L"保留原始功能（前进 / 后退）";
        case ActionType::Disabled:
            return L"已禁用";
    }
    return L"";
}

void Txt(HDC dc, int x, int y, const std::wstring& s) {
    TextOutW(dc, S(x), S(y), s.c_str(), static_cast<int>(s.size()));
}

void SyncChecks(HWND) {
    SendMessageW(g_chkEnabled, BM_SETCHECK,
                 app::g_cfg.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_chkAuto, BM_SETCHECK,
                 app::g_cfg.autostart ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ---------------- 主设置窗口 ----------------

void OnPaint(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);

    HBRUSH bg = CreateSolidBrush(RGB(246, 247, 249));
    FillRect(dc, &rc, bg);
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(225, 227, 230));
    HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, white));
    HPEN op = static_cast<HPEN>(SelectObject(dc, pen));
    const int cards[2][4] = {{16, 84, 368, 152}, {16, 248, 368, 120}};
    for (const auto& c : cards)
        RoundRect(dc, S(c[0]), S(c[1]), S(c[0] + c[2]), S(c[1] + c[3]), S(12), S(12));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
    DeleteObject(white);
    DeleteObject(bg);

    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_fontTitle);
    SetTextColor(dc, RGB(23, 26, 29));
    Txt(dc, 20, 12, L"MouseSide");
    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(110, 118, 129));
    Txt(dc, 142, 22, std::wstring(L"V") + app::kAppVersion);

    COLORREF dot = RGB(158, 166, 176);
    std::wstring status = L"映射已暂停";
    if (app::g_hookFailed) {
        dot = RGB(207, 54, 44);
        status = L"监听初始化失败";
    } else if (app::g_cfg.enabled) {
        dot = RGB(31, 148, 66);
        status = L"映射已启用";
    }
    HBRUSH db = CreateSolidBrush(dot);
    HBRUSH odb = static_cast<HBRUSH>(SelectObject(dc, db));
    Ellipse(dc, S(20), S(50), S(31), S(61));
    SelectObject(dc, odb);
    DeleteObject(db);
    Txt(dc, 40, 48, status);

    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(23, 26, 29));
    Txt(dc, 36, 102, L"鼠标第四按钮 [4]");
    Txt(dc, 36, 178, L"鼠标第五按钮 [5]");
    SetTextColor(dc, RGB(110, 118, 129));
    Txt(dc, 36, 126, MappingDesc(app::g_cfg.btn4));
    Txt(dc, 36, 202, MappingDesc(app::g_cfg.btn5));

    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(140, 148, 158));
    Txt(dc, 20, 382, L"在管理员窗口或 UAC 安全桌面中映射不会生效。");
    Txt(dc, 20, 400, L"关闭窗口后驻留托盘；右键托盘图标可退出。");
    EndPaint(h, &ps);
}

HWND Mk(HWND parent, const wchar_t* cls, DWORD style, int x, int y, int w, int hgt,
        int id, const wchar_t* txt, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                             S(x), S(y), S(w), S(hgt), parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             app::g_hInst, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return c;
}

void CreateChildren(HWND parent) {
    g_btnEdit4 = Mk(parent, L"button", BS_PUSHBUTTON, 304, 104, 64, 26, IDC_BTN_EDIT4, L"修改");
    g_btnEdit5 = Mk(parent, L"button", BS_PUSHBUTTON, 304, 180, 64, 26, IDC_BTN_EDIT5, L"修改");
    g_chkEnabled = Mk(parent, L"button", BS_AUTOCHECKBOX, 36, 264, 220, 20, IDC_CHK_ENABLED, L"启用侧键映射");
    g_chkAuto = Mk(parent, L"button", BS_AUTOCHECKBOX, 36, 292, 220, 20, IDC_CHK_AUTO, L"开机自动启动");
    g_btnReset = Mk(parent, L"button", BS_PUSHBUTTON, 36, 322, 110, 28, IDC_BTN_RESET, L"恢复默认配置");
}

void EditMapping(HWND parent, bool isBtn4);

LRESULT CALLBACK UIProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            OnPaint(h);
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(23, 26, 29));
            return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_BTN_EDIT4: EditMapping(h, true); return 0;
                case IDC_BTN_EDIT5: EditMapping(h, false); return 0;
                case IDC_CHK_ENABLED:
                    app::SetEnabled(SendMessageW(g_chkEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    return 0;
                case IDC_CHK_AUTO:
                    app::g_cfg.autostart =
                        SendMessageW(g_chkAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    ApplyAutostart(app::g_cfg.autostart);
                    SaveConfig(app::g_cfg);
                    return 0;
                case IDC_BTN_RESET:
                    if (MessageBoxW(h, L"恢复默认配置？两个侧键将重新映射为粘贴 / 复制。",
                                    L"MouseSide", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        ResetConfig(&app::g_cfg);
                        ApplyAutostart(app::g_cfg.autostart);
                        SaveConfig(app::g_cfg);
                        tray::UpdateIcon();
                        SyncChecks(h);
                        InvalidateRect(h, nullptr, TRUE);
                    }
                    return 0;
            }
            return 0;
        case WM_CLOSE:
            ShowWindow(h, SW_HIDE);  // 关闭窗口 = 隐藏到托盘
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---------------- 映射对话框 ----------------

struct MapDlg {
    HWND h = nullptr, cmb = nullptr, rec = nullptr, btnClear = nullptr;
    bool isBtn4 = true;
    ActionType type = ActionType::Preset;
    PresetId preset = PresetId::Paste;
    Hotkey hotkey;
    Hotkey recBackup;
    bool recording = false;
    bool ok = false;
    WNDPROC recOrig = nullptr;
    std::vector<PresetId> presetOrder;
};
MapDlg g_md;

constexpr wchar_t kRecHint[] = L"点击此处，然后按下快捷键";

std::wstring ModsText(const Hotkey& hk) {
    std::wstring s;
    if (hk.ctrl) s += L"Ctrl+";
    if (hk.shift) s += L"Shift+";
    if (hk.alt) s += L"Alt+";
    if (hk.win) s += L"Win+";
    return s;
}

void ShowRec() {
    std::wstring t;
    if (g_md.recording) {
        t = ModsText(g_md.hotkey);
        if (t.empty()) t = L"请按下快捷键（Esc 取消）";
    } else {
        t = g_md.hotkey.valid() ? HotkeyToText(g_md.hotkey) : kRecHint;
    }
    SetWindowTextW(g_md.rec, t.c_str());
}

LRESULT CALLBACK RecProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;
        case WM_LBUTTONDOWN:
            SetFocus(h);
            if (!g_md.recording) {
                g_md.recording = true;
                g_md.recBackup = g_md.hotkey;
                g_md.hotkey = Hotkey{};
                ShowRec();
            }
            return 0;
        case WM_KILLFOCUS:
            if (g_md.recording) {
                g_md.recording = false;
                g_md.hotkey = g_md.recBackup;
                ShowRec();
            }
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (!g_md.recording) break;
            const UINT vk = static_cast<UINT>(wp);
            bool* mod = nullptr;
            switch (vk) {
                case VK_CONTROL: mod = &g_md.hotkey.ctrl; break;
                case VK_SHIFT: mod = &g_md.hotkey.shift; break;
                case VK_MENU: mod = &g_md.hotkey.alt; break;
                case VK_LWIN: case VK_RWIN: mod = &g_md.hotkey.win; break;
            }
            if (mod) {
                *mod = true;
                ShowRec();
                return 0;
            }
            if (vk == VK_ESCAPE) {
                g_md.recording = false;
                g_md.hotkey = g_md.recBackup;
                ShowRec();
                return 0;
            }
            g_md.hotkey.vk = vk;
            g_md.recording = false;
            ShowRec();
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if (!g_md.recording) break;
            switch (static_cast<UINT>(wp)) {
                case VK_CONTROL: g_md.hotkey.ctrl = false; break;
                case VK_SHIFT: g_md.hotkey.shift = false; break;
                case VK_MENU: g_md.hotkey.alt = false; break;
                case VK_LWIN: case VK_RWIN: g_md.hotkey.win = false; break;
                default: break;
            }
            ShowRec();
            return 0;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
            return 0;
    }
    return CallWindowProcW(g_md.recOrig, h, msg, wp, lp);
}

void UpdateEnables() {
    EnableWindow(g_md.cmb, g_md.type == ActionType::Preset);
    EnableWindow(g_md.rec, g_md.type == ActionType::Keys);
    EnableWindow(g_md.btnClear, g_md.type == ActionType::Keys);
}

bool FinishDlg() {
    if (g_md.type == ActionType::Keys && !g_md.hotkey.valid()) {
        MessageBoxW(g_md.h, L"请先在输入框中录制自定义快捷键。", L"MouseSide",
                    MB_OK | MB_ICONWARNING);
        SetFocus(g_md.rec);
        return false;
    }
    ButtonConfig& b = BtnRefMut(g_md.isBtn4);
    b.type = g_md.type;
    b.preset = g_md.preset;
    b.hotkey = g_md.hotkey;
    g_md.ok = true;
    return true;
}

LRESULT CALLBACK DlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

void EnsureDlgClass() {
    static bool done = false;
    if (done) return;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = app::g_hInst;
    wc.hIcon = LoadIconW(app::g_hInst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kDlgClass;
    RegisterClassW(&wc);
    done = true;
}

LRESULT CALLBACK DlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            switch (id) {
                case IDOK:
                    if (FinishDlg()) DestroyWindow(h);
                    return 0;
                case IDCANCEL:
                    DestroyWindow(h);
                    return 0;
                case ID_RDO_PRESET: case ID_RDO_KEYS:
                case ID_RDO_PASS: case ID_RDO_DIS:
                    if (code == BN_CLICKED) {
                        g_md.type = id == ID_RDO_PRESET ? ActionType::Preset
                                  : id == ID_RDO_KEYS ? ActionType::Keys
                                  : id == ID_RDO_PASS ? ActionType::Passthrough
                                                      : ActionType::Disabled;
                        UpdateEnables();
                    }
                    return 0;
                case ID_CMB_PRESET:
                    if (code == CBN_SELCHANGE) {
                        const int sel = static_cast<int>(
                            SendMessageW(g_md.cmb, CB_GETCURSEL, 0, 0));
                        if (sel >= 0 && sel < static_cast<int>(g_md.presetOrder.size()))
                            g_md.preset = g_md.presetOrder[sel];
                    }
                    return 0;
                case ID_BTN_CLEAR:
                    g_md.hotkey = Hotkey{};
                    ShowRec();
                    return 0;
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == ID_HINT
                                 ? RGB(110, 118, 129)
                                 : RGB(23, 26, 29));
            return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

bool RunMappingDialog(HWND parent, bool isBtn4) {
    EnsureDlgClass();
    g_md.isBtn4 = isBtn4;
    const ButtonConfig& b = BtnRef(isBtn4);
    g_md.type = b.type;
    g_md.preset = b.preset;
    g_md.hotkey = b.hotkey;
    g_md.recording = false;
    g_md.ok = false;
    g_md.presetOrder.clear();

    RECT rc = {0, 0, S(392), S(288)};
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    RECT pr;
    GetWindowRect(parent, &pr);
    const int x = pr.left + ((pr.right - pr.left) - (rc.right - rc.left)) / 2;
    const int y = pr.top + ((pr.bottom - pr.top) - (rc.bottom - rc.top)) / 2;
    const std::wstring title = isBtn4 ? L"配置鼠标第四按钮 [4]" : L"配置鼠标第五按钮 [5]";
    g_md.h = CreateWindowExW(WS_EX_CONTROLPARENT, kDlgClass, title.c_str(), style,
                             x, y, rc.right - rc.left, rc.bottom - rc.top,
                             parent, nullptr, app::g_hInst, nullptr);
    if (!g_md.h) return false;

    HWND rdoPreset = Mk(g_md.h, L"button", BS_AUTORADIOBUTTON | WS_GROUP, 20, 16, 220, 20, ID_RDO_PRESET, L"预设功能");
    g_md.cmb = Mk(g_md.h, L"combobox", CBS_DROPDOWNLIST, 40, 42, 300, 160, ID_CMB_PRESET, L"");
    HWND rdoKeys = Mk(g_md.h, L"button", BS_AUTORADIOBUTTON | WS_GROUP, 20, 76, 220, 20, ID_RDO_KEYS, L"自定义快捷键");
    g_md.rec = Mk(g_md.h, L"edit", ES_READONLY, 40, 102, 250, 26, ID_REC, kRecHint, WS_EX_CLIENTEDGE);
    g_md.recOrig = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_md.rec, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RecProc)));
    g_md.btnClear = Mk(g_md.h, L"button", BS_PUSHBUTTON, 298, 102, 70, 26, ID_BTN_CLEAR, L"清除");
    Mk(g_md.h, L"static", 0, 40, 136, 320, 32, ID_HINT, L"点击输入框后按下组合键，Esc 取消录制。");
    HWND rdoPass = Mk(g_md.h, L"button", BS_AUTORADIOBUTTON | WS_GROUP, 20, 172, 340, 20, ID_RDO_PASS, L"保留鼠标原始功能（前进 / 后退）");
    HWND rdoDis = Mk(g_md.h, L"button", BS_AUTORADIOBUTTON | WS_GROUP, 20, 200, 340, 20, ID_RDO_DIS, L"禁用此侧键");
    Mk(g_md.h, L"button", BS_DEFPUSHBUTTON, 192, 244, 88, 28, IDOK, L"确定");
    Mk(g_md.h, L"button", BS_PUSHBUTTON, 288, 244, 88, 28, IDCANCEL, L"取消");

    struct Ctx { HWND cmb; std::vector<PresetId>* order; PresetId sel; int selIdx; }
        ctx{g_md.cmb, &g_md.presetOrder, g_md.preset, 0};
    mapper::EachPreset([](PresetId id, const std::wstring& name, void* p) {
        auto* c = static_cast<Ctx*>(p);
        const int i = static_cast<int>(SendMessageW(c->cmb, CB_ADDSTRING, 0,
                                                    reinterpret_cast<LPARAM>(name.c_str())));
        c->order->push_back(id);
        if (id == c->sel) c->selIdx = i;
    }, &ctx);
    SendMessageW(g_md.cmb, CB_SETCURSEL, ctx.selIdx, 0);

    auto setr = [&](HWND c, bool on) {
        SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    };
    setr(rdoPreset, g_md.type == ActionType::Preset);
    setr(rdoKeys, g_md.type == ActionType::Keys);
    setr(rdoPass, g_md.type == ActionType::Passthrough);
    setr(rdoDis, g_md.type == ActionType::Disabled);
    ShowRec();
    UpdateEnables();

    EnableWindow(parent, FALSE);
    ShowWindow(g_md.h, SW_SHOW);
    SetForegroundWindow(g_md.h);
    SetFocus(g_md.type == ActionType::Keys ? g_md.rec : rdoPreset);

    MSG m;
    while (IsWindow(g_md.h) && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(g_md.h, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    g_md.h = nullptr;
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return g_md.ok;
}

void EditMapping(HWND parent, bool isBtn4) {
    if (RunMappingDialog(parent, isBtn4)) {
        SaveConfig(app::g_cfg);
        ui::Refresh();
    }
}

}  // namespace

namespace ui {

bool CreateAll() {
    static bool created = false;
    if (created) return true;

    HDC sdc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(nullptr, sdc);
    if (g_dpi <= 0) g_dpi = 96;

    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfHeight = -MulDiv(15, g_dpi, 72);
    lf.lfWeight = FW_SEMIBOLD;
    g_fontTitle = CreateFontIndirectW(&lf);

    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = UIProc;
    wc.hInstance = app::g_hInst;
    wc.hIcon = LoadIconW(app::g_hInst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kUIClass;
    RegisterClassW(&wc);

    RECT rc = {0, 0, S(400), S(430)};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    app::g_ui = CreateWindowExW(WS_EX_CONTROLPARENT, kUIClass, L"MouseSide 设置", style,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, app::g_hInst, nullptr);
    if (!app::g_ui) return false;
    CreateChildren(app::g_ui);
    SyncChecks(app::g_ui);
    created = true;
    return true;
}

void Show() {
    if (!app::g_ui) return;
    ShowWindow(app::g_ui, SW_SHOWNORMAL);
    Refresh();
    SetForegroundWindow(app::g_ui);
}

void Refresh() {
    if (!app::g_ui) return;
    SyncChecks(app::g_ui);
    InvalidateRect(app::g_ui, nullptr, TRUE);
}

}  // namespace ui
