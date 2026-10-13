// Compile the real implementation into a test-only translation unit so private
// window/hook callbacks can be exercised without exposing test APIs in the app.
#include "Common.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace test {
bool keys[256]{};
std::vector<std::vector<INPUT>> batches;
int sendMode = 0; // 0=success, 1=failure, 2=partial first batch
int boxes = 0, menus = 0, adds = 0;
bool shellFails = false;
bool versionFails = false;
std::filesystem::path profile;
SHORT WINAPI Async(int vk) { return keys[vk & 255] ? SHORT(0x8000) : 0; }
UINT WINAPI Send(UINT count, LPINPUT inputs, int size) {
    if (size != sizeof(INPUT)) return 0;
    batches.emplace_back(inputs, inputs + count);
    if (sendMode == 1) { SetLastError(ERROR_ACCESS_DENIED); return 0; }
    if (sendMode == 2 && batches.size() == 1) return std::min(count, UINT(1));
    return count;
}
int WINAPI Box(HWND, LPCWSTR, LPCWSTR, UINT) { ++boxes; return IDOK; }
BOOL WINAPI Shell(DWORD message, PNOTIFYICONDATAW) {
    if (message == NIM_ADD) ++adds;
    return !shellFails && !(message == NIM_SETVERSION && versionFails);
}
HHOOK WINAPI Hook(int, HOOKPROC, HINSTANCE, DWORD) { return reinterpret_cast<HHOOK>(1); }
BOOL WINAPI Unhook(HHOOK) { return TRUE; }
BOOL WINAPI Menu(HMENU, UINT, int, int, int, HWND, const RECT*) { ++menus; return TRUE; }
BOOL WINAPI Foreground(HWND) { return TRUE; }
BOOL WINAPI Show(HWND, int) { return TRUE; }
HRESULT WINAPI Folder(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* out) {
    const auto str = profile.wstring();
    *out = static_cast<PWSTR>(CoTaskMemAlloc((str.size() + 1) * sizeof(wchar_t)));
    if (!*out) return E_OUTOFMEMORY;
    std::copy(str.c_str(), str.c_str() + str.size() + 1, *out);
    return S_OK;
}
void Check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
void Pump() { MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message != WM_QUIT) { TranslateMessage(&msg); DispatchMessageW(&msg); }
} }
}

#define GetAsyncKeyState test::Async
#define SendInput test::Send
#define MessageBoxW test::Box
#define Shell_NotifyIconW test::Shell
#define TrackPopupMenu test::Menu
#define SetForegroundWindow test::Foreground
#define ShowWindow test::Show
#define SHGetKnownFolderPath test::Folder
#define SetWindowsHookExW test::Hook
#define UnhookWindowsHookEx test::Unhook
#include "../src/Config.cpp"
#include "../src/KeyMapper.cpp"
#include "../src/MouseHook.cpp"
#include "../src/SettingsWindow.cpp"
#include "../src/TrayUI.cpp"
#include "../src/main.cpp"

LRESULT CaptureKey(unsigned vk, bool up, bool pump = true) {
    KBDLLHOOKSTRUCT info{}; info.vkCode = vk;
    const LRESULT result = ms::settings::CaptureKeyProc(HC_ACTION, up ? WM_KEYUP : WM_KEYDOWN,
                                                       reinterpret_cast<LPARAM>(&info));
    if (pump) test::Pump();
    return result;
}
void BeginCapture() {
    using namespace ms::settings;
    g_capture = CaptureState{};
    g_capture.active = true;
    g_capture.hwnd = CreateWindowExW(0, ms::kCaptureClass, L"test", 0,
                                     0, 0, 340, 170, nullptr, nullptr, ms::g_hInst, nullptr);
    test::Check(g_capture.hwnd != nullptr, "capture test window creation");
}
bool HasEvent(WORD vk, bool up) {
    for (const auto& batch : test::batches) for (const auto& input : batch)
        if (input.ki.wVk == vk && bool(input.ki.dwFlags & KEYEVENTF_KEYUP) == up) return true;
    return false;
}
int wmain() {
    using namespace ms;
    test::profile = std::filesystem::temp_directory_path() /
                    (L"MouseSide-regression-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(test::profile);
    g_hInst = GetModuleHandleW(nullptr);
    RegisterHiddenClass(); settings::Register(g_hInst);
    g_hidden = CreateWindowExW(0, kHiddenClass, L"test", 0, 0, 0, 0, 0,
                               nullptr, nullptr, g_hInst, nullptr);
    config::ResetToDefault(g_config);
    int failures = 0, passed = 0;
    const auto run = [&](const char* name, auto fn) {
        try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cout << "FAIL " << name << ": " << e.what() << '\n'; }
        test::Pump();
        if (settings::g_capture.active) settings::CaptureFinish(false);
        test::Pump();
        std::fill(std::begin(test::keys), std::end(test::keys), false);
        test::batches.clear(); test::sendMode = 0;
    };
    run("V4 tray double click opens settings", [&] {
        tray::Add(g_hidden, g_hInst);
        HiddenProc(g_hidden, WM_MS_TRAY, 0, MAKELPARAM(WM_LBUTTONDBLCLK, 1));
        test::Check(settings::g_hwnd != nullptr, "settings did not open");
    });
    run("V4 tray context menu is reachable", [&] {
        const int before = test::menus;
        HiddenProc(g_hidden, WM_MS_TRAY, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
        test::Check(test::menus == before + 1, "menu did not open");
    });
    run("Explorer recreation re-registers tray", [&] {
        const int before = test::adds;
        HiddenProc(g_hidden, RegisterWindowMessageW(L"TaskbarCreated"), 0, 0);
        test::Check(test::adds == before + 1, "icon not re-added");
    });
    run("Legacy tray fallback still opens settings", [&] {
        settings::CloseAll(); test::versionFails = true; tray::Add(g_hidden, g_hInst);
        HiddenProc(g_hidden, WM_MS_TRAY, 1, WM_LBUTTONDBLCLK);
        const bool opened = settings::g_hwnd != nullptr;
        test::versionFails = false; tray::Restore();
        test::Check(opened, "legacy fallback ignored double click");
    });
    run("Failed tray recovery presents settings and an error", [&] {
        settings::CloseAll(); const int before = test::boxes; test::shellFails = true;
        HiddenProc(g_hidden, RegisterWindowMessageW(L"TaskbarCreated"), 0, 0);
        const bool shown = settings::g_hwnd && test::boxes == before + 1;
        test::shellFails = false; tray::Restore();
        test::Check(shown, "lost tray without an independent control surface");
    });
    run("Ctrl Shift S recording preserves modifiers and waits for release", [&] {
        BeginCapture(); CaptureKey(VK_LCONTROL, false); CaptureKey(VK_LSHIFT, false);
        CaptureKey('S', false);
        test::Check(settings::g_capture.hotkey.ctrl && settings::g_capture.hotkey.shift,
                    "recorded a bare key instead of Ctrl Shift S");
        test::Check(settings::g_capture.active, "finished before key releases");
        CaptureKey('S', true); CaptureKey(VK_LCONTROL, true); CaptureKey(VK_LSHIFT, true);
        test::Check(!settings::g_capture.active, "recording did not finish");
    });
    run("Released modifier is excluded from recording", [&] {
        BeginCapture(); CaptureKey(VK_RCONTROL, false); CaptureKey(VK_RCONTROL, true);
        CaptureKey(VK_RWIN, false); CaptureKey('D', false);
        test::Check(!settings::g_capture.hotkey.ctrl && settings::g_capture.hotkey.win,
                    "right Win or released Ctrl recorded incorrectly");
        CaptureKey('D', true); CaptureKey(VK_RWIN, true);
    });
    run("Ctrl Backspace is recordable", [&] {
        BeginCapture(); CaptureKey(VK_LCONTROL, false); CaptureKey(VK_BACK, false);
        test::Check(settings::g_capture.hotkey.vk == VK_BACK && settings::g_capture.hotkey.ctrl,
                    "Backspace cancelled recording");
        CaptureKey(VK_BACK, true); CaptureKey(VK_LCONTROL, true);
    });
    run("Recording includes a Ctrl held before capture starts", [&] {
        test::keys[VK_RCONTROL] = true;
        settings::CaptureStart(settings::g_hwnd, settings::kIdCombo4);
        CaptureKey('S', false);
        test::Check(settings::g_capture.hotkey.ctrl, "initially held Ctrl was omitted");
        CaptureKey('S', true);
        test::Check(!settings::g_capture.active, "waited for a modifier that was never swallowed");
    });
    run("Released preheld modifier aliases do not contaminate recording", [&] {
        test::keys[VK_CONTROL] = test::keys[VK_RCONTROL] = true;
        settings::CaptureStart(settings::g_hwnd, settings::kIdCombo4);
        test::keys[VK_CONTROL] = test::keys[VK_RCONTROL] = false;
        CaptureKey(VK_RCONTROL, true); CaptureKey('S', false); CaptureKey('S', true);
        test::Check(g_config.button4.kind == ActionKind::Custom &&
                    g_config.button4.hotkey.vk == 'S' && !g_config.button4.hotkey.ctrl,
                    "released Ctrl alias remained stuck in recording state");
    });
    run("Escape cancels without changing mappings", [&] {
        BeginCapture(); CaptureKey(VK_ESCAPE, false); CaptureKey(VK_ESCAPE, true);
        test::Check(!settings::g_capture.active && settings::g_capture.hotkey.IsEmpty(),
                    "Escape did not cancel");
    });
    run("Escape can cancel a captured chord before release", [&] {
        g_config.button4 = Mapping{ActionKind::Preset, PresetAction::Paste, {}};
        settings::CaptureStart(settings::g_hwnd, settings::kIdCombo4);
        CaptureKey(VK_LCONTROL, false); CaptureKey('S', false); CaptureKey(VK_ESCAPE, false);
        CaptureKey(VK_ESCAPE, true); CaptureKey('S', true); CaptureKey(VK_LCONTROL, true);
        test::Check(!settings::g_capture.active && g_config.button4.kind == ActionKind::Preset &&
                    g_config.button4.preset == PresetAction::Paste, "cancelled chord was saved");
    });
    run("Window-close cancellation drains swallowed key releases", [&] {
        BeginCapture(); CaptureKey(VK_LWIN, false);
        settings::CaptureProc(settings::g_capture.hwnd, WM_CLOSE, 0, 0);
        test::Check(settings::g_capture.active, "close removed hook before swallowed Win release");
        test::Check(CaptureKey('A', false) == 0 && CaptureKey('A', true) == 0,
                    "cancelled recording blocked fresh keyboard input");
        CaptureKey(VK_LWIN, true);
        test::Check(!settings::g_capture.active, "close did not finish after Win release");
    });
    run("Timeout cancellation drains swallowed key releases", [&] {
        BeginCapture(); CaptureKey(VK_LCONTROL, false);
        settings::CaptureProc(settings::g_capture.hwnd, WM_TIMER, 1, 0);
        test::Check(settings::g_capture.active, "timeout removed hook before swallowed Ctrl release");
        CaptureKey(VK_LCONTROL, true);
        test::Check(!settings::g_capture.active, "timeout did not finish after Ctrl release");
    });
    run("Closing settings during cancelled release drain keeps cleanup alive", [&] {
        settings::CaptureStart(settings::g_hwnd, settings::kIdCombo4);
        CaptureKey(VK_LCONTROL, false);
        settings::CaptureProc(settings::g_capture.hwnd, WM_CLOSE, 0, 0);
        DestroyWindow(settings::g_hwnd);
        test::Check(settings::g_capture.active && IsWindow(settings::g_capture.hwnd),
                    "settings teardown destroyed pending release drain");
        test::Check(CaptureKey(VK_LCONTROL, true) == 1 && !settings::g_capture.active,
                    "pending Ctrl release leaked or did not clean up");
        settings::Show(g_hidden);
    });
    run("Late Escape overrides an already queued successful result", [&] {
        g_config.button4 = Mapping{ActionKind::Preset, PresetAction::Paste, {}};
        settings::CaptureStart(settings::g_hwnd, settings::kIdCombo4);
        CaptureKey(VK_LCONTROL, false, false); CaptureKey('S', false, false);
        CaptureKey('S', true, false); CaptureKey(VK_LCONTROL, true, false);
        CaptureKey(VK_ESCAPE, false, false); test::Pump(); CaptureKey(VK_ESCAPE, true);
        test::Check(!settings::g_capture.active && g_config.button4.kind == ActionKind::Preset,
                    "queued success ignored a later cancellation");
    });
    run("Copy keeps a physically held right Ctrl down", [&] {
        test::keys[VK_RCONTROL] = test::keys[VK_CONTROL] = true;
        keymap::Execute(Mapping{ActionKind::Preset, PresetAction::Copy, {}});
        test::Check(!HasEvent(VK_CONTROL, true) && !HasEvent(VK_RCONTROL, true) &&
                    !HasEvent(VK_LCONTROL, true), "released the user's held Ctrl");
        test::Check(HasEvent('C', false) && HasEvent('C', true), "copy key not balanced");
    });
    run("Unwanted right Win is released and restored", [&] {
        test::keys[VK_RWIN] = true;
        keymap::Execute(Mapping{ActionKind::Preset, PresetAction::Copy, {}});
        const std::vector<std::pair<WORD, bool>> wanted = {
            {VK_RWIN, true}, {VK_LCONTROL, false}, {'C', false},
            {'C', true}, {VK_LCONTROL, true}, {VK_RWIN, false}};
        std::vector<std::pair<WORD, bool>> actual;
        for (const auto& batch : test::batches) for (const auto& input : batch)
            actual.emplace_back(input.ki.wVk, (input.ki.dwFlags & KEYEVENTF_KEYUP) != 0);
        test::Check(actual == wanted, "right Win release/shortcut/restore ordering is incorrect");
    });
    run("Held main key does not create an extra unmodified keypress", [&] {
        test::keys['C'] = true;
        keymap::Execute(Mapping{ActionKind::Preset, PresetAction::Copy, {}});
        int downs = 0, ups = 0;
        for (const auto& batch : test::batches) for (const auto& input : batch)
            if (input.ki.wVk == 'C') { if (input.ki.dwFlags & KEYEVENTF_KEYUP) ++ups; else ++downs; }
        test::Check(downs == 1 && ups == 0, "released and re-pressed the user's main key");
    });
    run("Modifier-only INI shortcut is rejected", [&] {
        Mapping mapping{};
        test::Check(!DeserializeMapping(L"custom:17", mapping), "Ctrl alone accepted as a shortcut main key");
    });
    run("Extended navigation keys carry extended flags", [&] {
        keymap::Execute(Mapping{ActionKind::Custom, PresetAction::Copy, {VK_LEFT, false, false, false, false}});
        test::Check(!test::batches.empty(), "no input");
        for (const auto& in : test::batches.front()) if (in.ki.wVk == VK_LEFT)
            test::Check(in.ki.dwFlags & KEYEVENTF_EXTENDEDKEY, "missing extended-key flag");
    });
    run("Input failure pauses mapping and reports once", [&] {
        g_config.enabled = true; test::sendMode = 1; const int before = test::boxes;
        HiddenProc(g_hidden, WM_MS_EXEC_MAPPING, 0, 0);
        test::Check(!g_config.enabled && test::boxes == before + 1, "input failure was ignored");
        HiddenProc(g_hidden, WM_MS_EXEC_MAPPING, 0, 0);
        test::Check(test::boxes == before + 1, "queued mappings repeated the error");
    });
    run("Pause preserves the swallowed down-up pair", [&] {
        mousehook::Uninstall(); mousehook::g_target = g_hidden;
        mousehook::SetMappings(g_config.button4, g_config.button5);
        mousehook::SetEnabled(true);
        MSLLHOOKSTRUCT info{}; info.mouseData = DWORD(XBUTTON1) << 16;
        test::Check(mousehook::LowLevelMouseProc(HC_ACTION, WM_XBUTTONDOWN,
                    reinterpret_cast<LPARAM>(&info)) == 1, "down was not blocked");
        mousehook::SetEnabled(false);
        test::Check(mousehook::LowLevelMouseProc(HC_ACTION, WM_XBUTTONUP,
                    reinterpret_cast<LPARAM>(&info)) == 1, "orphan up leaked after pause");
    });
    run("Failed mapping message leaves the original side key available", [&] {
        mousehook::Uninstall(); mousehook::g_target = reinterpret_cast<HWND>(1);
        mousehook::SetMappings(g_config.button4, g_config.button5);
        mousehook::SetEnabled(true);
        MSLLHOOKSTRUCT info{}; info.mouseData = DWORD(XBUTTON1) << 16;
        const LRESULT blocked = mousehook::LowLevelMouseProc(HC_ACTION, WM_XBUTTONDOWN,
                                    reinterpret_cast<LPARAM>(&info));
        mousehook::LowLevelMouseProc(HC_ACTION, WM_XBUTTONUP, reinterpret_cast<LPARAM>(&info));
        test::Check(blocked == 0, "failed PostMessage swallowed the side key");
    });
    run("Partial input attempts modifier cleanup", [&] {
        test::sendMode = 2;
        keymap::Execute(Mapping{ActionKind::Preset, PresetAction::Copy, {}});
        test::Check(test::batches.size() == 2 && test::batches[1].size() == 1 &&
                    test::batches[1][0].ki.wVk == VK_LCONTROL &&
                    (test::batches[1][0].ki.dwFlags & KEYEVENTF_KEYUP),
                    "partial input did not release the injected Ctrl");
    });
    run("Partial input restores a neutralized held modifier", [&] {
        test::keys[VK_RSHIFT] = true; test::sendMode = 2;
        keymap::Execute(Mapping{ActionKind::Preset, PresetAction::Copy, {}});
        test::Check(test::batches.size() == 2 && test::batches[1].size() == 1 &&
                    test::batches[1][0].ki.wVk == VK_RSHIFT &&
                    !(test::batches[1][0].ki.dwFlags & KEYEVENTF_KEYUP),
                    "partial input did not restore held right Shift");
    });
    run("Read-only INI save fails and preserves old mapping", [&] {
        config::ResetToDefault(g_config); test::Check(config::Save(g_config), "initial save");
        const auto path = config::Path();
        test::Check(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY), "mark INI readonly");
        auto changed = g_config; changed.button4.preset = PresetAction::Cut;
        const bool saved = config::Save(changed);
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        test::Check(!saved, "save falsely reported success");
        AppConfig reloaded{}; config::Load(reloaded);
        test::Check(reloaded.button4.preset == PresetAction::Paste, "readonly config was altered");
    });
    run("Configuration update surfaces save failure", [&] {
        const auto path = config::Path(); SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY);
        const int before = test::boxes; ApplyConfig();
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        test::Check(test::boxes == before + 1, "save failure was hidden");
    });
    run("Settings has an exit route independent of tray", [&] {
        settings::Show(g_hidden); HWND exitButton = nullptr;
        for (HWND child = GetWindow(settings::g_hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            wchar_t text[64]{}; GetWindowTextW(child, text, 64);
            if (std::wstring(text) == L"退出程序") exitButton = child;
        }
        test::Check(exitButton != nullptr, "settings has no exit button");
        SendMessageW(settings::g_hwnd, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(exitButton), BN_CLICKED),
                     reinterpret_cast<LPARAM>(exitButton));
        test::Pump();
        test::Check(!IsWindow(g_hidden), "exit button did not terminate app windows");
    });
    if (IsWindow(g_hidden)) DestroyWindow(g_hidden);
    std::filesystem::remove_all(test::profile);
    std::cout << passed << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
