#include "Config.h"

#include <objbase.h>
#include <shlobj.h>

#include <iterator>
#include <string>
#include <vector>

namespace ms {

namespace config {
// 前向声明：匿名 namespace 中的辅助函数需要使用
std::wstring Path();
}  // namespace config

namespace {

constexpr wchar_t kCompanyDir[] = L"MouseSide";
constexpr wchar_t kConfigFile[] = L"config.ini";
constexpr wchar_t kSection[] = L"MouseSide";

std::wstring KnownFolderPath(REFKNOWNFOLDERID id) {
    std::wstring path;
    wchar_t* raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw)) && raw) {
        path = raw;
        CoTaskMemFree(raw);
    }
    return path;
}

std::wstring LocalAppDataDir() {
    std::wstring dir = KnownFolderPath(FOLDERID_LocalAppData);
    if (dir.empty()) {
        wchar_t buf[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH) && buf[0]) {
            dir = buf;
        } else {
            wchar_t tmp[MAX_PATH]{};
            if (GetTempPathW(MAX_PATH, tmp) && tmp[0]) dir = tmp;
        }
    }
    return dir;
}

std::wstring Lower(std::wstring s) {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<wchar_t>(c - 'A' + 'a');
    }
    return s;
}

std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t start = 0;
    for (;;) {
        size_t pos = s.find(sep, start);
        if (pos == std::wstring::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::wstring Trim(std::wstring s) {
    auto isSpace = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
    size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b])) ++b;
    while (e > b && isSpace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// 自定义快捷键文本：<vk> 或 [ctrl+][alt+][shift+][win+]<vk>
bool ParseHotkeyText(const std::wstring& text, Hotkey& out) {
    Hotkey hk{};
    auto parts = Split(Lower(text), L'+');
    bool vkSeen = false;
    for (const auto& rawPart : parts) {
        const std::wstring part = Trim(rawPart);
        if (part.empty()) continue;
        if (part == L"ctrl" || part == L"control") { hk.ctrl = true; continue; }
        if (part == L"alt" || part == L"menu")     { hk.alt = true; continue; }
        if (part == L"shift")                      { hk.shift = true; continue; }
        if (part == L"win" || part == L"lwin" || part == L"super") { hk.win = true; continue; }
        if (vkSeen) return false;
        unsigned value = 0;
        try {
            size_t idx = 0;
            long long parsed = 0;
            if (part.rfind(L"0x", 0) == 0) {
                parsed = std::stoll(part.substr(2), &idx, 16);
                idx += 2;
            } else {
                parsed = std::stoll(part, &idx, 10);
            }
            if (idx != part.size() || parsed <= 0 || parsed > 0xFF) return false;
            value = static_cast<unsigned>(parsed);
        } catch (...) {
            return false;
        }
        hk.vk = value;
        vkSeen = true;
    }
    if (!vkSeen) return false;
    out = hk;
    return true;
}

std::wstring FormatHotkeyText(const Hotkey& hk) {
    std::wstring s;
    if (hk.ctrl) s += L"ctrl+";
    if (hk.alt) s += L"alt+";
    if (hk.shift) s += L"shift+";
    if (hk.win) s += L"win+";
    s += std::to_wstring(hk.vk);
    return s;
}

// "preset:copy" / "custom:ctrl+shift+83" / "original" / "disabled"
std::wstring SerializeMapping(const Mapping& m) {
    switch (m.kind) {
        case ActionKind::Preset:   return std::wstring(L"preset:") + PresetLabel(m.preset);
        case ActionKind::Custom:   return std::wstring(L"custom:") + FormatHotkeyText(m.hotkey);
        case ActionKind::Original: return L"original";
        case ActionKind::Disabled: break;
    }
    return L"disabled";
}

bool DeserializeMapping(const std::wstring& raw, Mapping& out) {
    const std::wstring text = Lower(Trim(raw));
    if (text.empty()) return false;
    if (text == L"original") { out = Mapping{ActionKind::Original, PresetAction::Copy, {}}; return true; }
    if (text == L"disabled") { out = Mapping{ActionKind::Disabled, PresetAction::Copy, {}}; return true; }

    const size_t colon = text.find(L':');
    if (colon == std::wstring::npos) return false;
    const std::wstring kind = text.substr(0, colon);
    const std::wstring val = text.substr(colon + 1);

    if (kind == L"preset") {
        for (const PresetDef* p = PresetTable(); p && p->label; ++p) {
            if (Lower(p->label) == val) {
                out = Mapping{ActionKind::Preset, p->id, {}};
                return true;
            }
        }
        return false;
    }
    if (kind == L"custom") {
        Hotkey hk{};
        if (!ParseHotkeyText(val, hk)) return false;
        out = Mapping{ActionKind::Custom, PresetAction::Copy, hk};
        return true;
    }
    return false;
}

std::wstring ReadIni(const wchar_t* key, const std::wstring& fallback) {
    wchar_t buf[512]{};
    GetPrivateProfileStringW(kSection, key, fallback.c_str(), buf, static_cast<DWORD>(std::size(buf)),
                             config::Path().c_str());
    return std::wstring(buf);
}

}  // namespace

const PresetDef* PresetTable() {
    static const PresetDef table[] = {
        {PresetAction::Copy,           L"copy",            {0x43 /* C */, true,  false, false, false}},
        {PresetAction::Paste,          L"paste",           {0x56 /* V */, true,  false, false, false}},
        {PresetAction::Cut,            L"cut",             {0x58 /* X */, true,  false, false, false}},
        {PresetAction::Undo,           L"undo",            {0x5A /* Z */, true,  false, false, false}},
        {PresetAction::Redo,           L"redo",            {0x59 /* Y */, true,  false, false, false}},
        {PresetAction::SelectAll,      L"selectall",       {0x41 /* A */, true,  false, false, false}},
        {PresetAction::BrowserBack,    L"browserback",     {VK_BROWSER_BACK, false, false, false, false}},
        {PresetAction::BrowserForward, L"browserforward",  {VK_BROWSER_FORWARD, false, false, false, false}},
        {PresetAction::Copy,           nullptr,            {}},
    };
    return table;
}

Hotkey PresetHotkey(PresetAction a) {
    for (const PresetDef* p = PresetTable(); p && p->label; ++p) {
        if (p->id == a) return p->key;
    }
    return {};
}

const wchar_t* PresetLabel(PresetAction a) {
    for (const PresetDef* p = PresetTable(); p && p->label; ++p) {
        if (p->id == a) return p->label;
    }
    return L"copy";
}

namespace config {

std::wstring Path() {
    static std::wstring cached;
    if (cached.empty()) {
        std::wstring dir = LocalAppDataDir();
        if (!dir.empty()) dir += L'\\';
        dir += kCompanyDir;
        CreateDirectoryW(dir.c_str(), nullptr);
        cached = dir + L'\\' + kConfigFile;
    }
    return cached;
}

void ResetToDefault(AppConfig& cfg) {
    cfg.enabled = true;
    cfg.button4 = Mapping{ActionKind::Preset, PresetAction::Paste, {}};
    cfg.button5 = Mapping{ActionKind::Preset, PresetAction::Copy, {}};
    cfg.autoStart = false;
}

bool Load(AppConfig& cfg) {
    const std::wstring path = Path();
    const bool firstRun = (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES);

    ResetToDefault(cfg);

    // 损坏/缺失时按 0 处理，随后安全回落到默认值
    const auto readInt = [&](const wchar_t* key, int fallback) {
        const int v = GetPrivateProfileIntW(kSection, key, fallback, path.c_str());
        return v;
    };

    cfg.enabled = readInt(L"Enabled", 1) != 0;
    cfg.autoStart = readInt(L"AutoStart", 0) != 0;
    if (cfg.autoStart != GetAutoStart()) cfg.autoStart = GetAutoStart();

    Mapping m4{}, m5{};
    if (DeserializeMapping(ReadIni(L"Button4", L"preset:paste"), m4)) cfg.button4 = m4;
    if (DeserializeMapping(ReadIni(L"Button5", L"preset:copy"), m5)) cfg.button5 = m5;

    if (firstRun) Save(cfg);
    return firstRun;
}

bool Save(const AppConfig& cfg) {
    const std::wstring path = Path();
    const auto write = [&](const wchar_t* key, const std::wstring& value) {
        WritePrivateProfileStringW(kSection, key, value.c_str(), path.c_str());
    };
    write(L"Version", L"1");
    write(L"Enabled", cfg.enabled ? L"1" : L"0");
    write(L"AutoStart", cfg.autoStart ? L"1" : L"0");
    write(L"Button4", SerializeMapping(cfg.button4));
    write(L"Button5", SerializeMapping(cfg.button5));
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool GetAutoStart() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buf[MAX_PATH * 2]{};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG result = RegQueryValueExW(key, kRunValueName, nullptr, &type,
                                         reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool SetAutoStart(bool enable) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    LONG result = ERROR_SUCCESS;
    if (enable) {
        wchar_t exe[MAX_PATH * 2]{};
        GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
        std::wstring value = L"\"";
        value += exe;
        value += L"\" --startup";
        result = RegSetValueExW(key, kRunValueName, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(value.c_str()),
                                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunValueName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

}  // namespace config
}  // namespace ms
