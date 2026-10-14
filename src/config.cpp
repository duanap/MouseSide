#include "config.h"

#include <vector>

namespace {

struct PresetStr { PresetId id; const wchar_t* s; };

const PresetStr kPresetStrs[] = {
    {PresetId::Copy, L"Copy"},           {PresetId::Paste, L"Paste"},
    {PresetId::Cut, L"Cut"},             {PresetId::Undo, L"Undo"},
    {PresetId::Redo, L"Redo"},           {PresetId::SelectAll, L"SelectAll"},
    {PresetId::BrowserBack, L"BrowserBack"},
    {PresetId::BrowserForward, L"BrowserForward"},
};

const wchar_t* PresetToString(PresetId id) {
    for (const PresetStr& p : kPresetStrs)
        if (p.id == id) return p.s;
    return L"Paste";
}

bool PresetFromString(const std::wstring& s, PresetId* out) {
    for (const PresetStr& p : kPresetStrs)
        if (s == p.s) { *out = p.id; return true; }
    return false;
}

bool TypeFromString(const std::wstring& s, ActionType* out) {
    if (s == L"Preset") *out = ActionType::Preset;
    else if (s == L"Keys") *out = ActionType::Keys;
    else if (s == L"Passthrough") *out = ActionType::Passthrough;
    else if (s == L"Disabled") *out = ActionType::Disabled;
    else return false;
    return true;
}

std::wstring ExePath() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, n);
}

std::wstring ReadStr(const wchar_t* section, const wchar_t* key,
                     const wchar_t* def, const std::wstring& path) {
    wchar_t buf[256];
    GetPrivateProfileStringW(section, key, def, buf, 256, path.c_str());
    return buf;
}

void LoadButton(const wchar_t* section, ButtonConfig* b, PresetId defPreset,
                const std::wstring& path) {
    SetButtonDefault(b, defPreset == PresetId::Paste);
    const ActionType defType = ActionType::Preset;
    ActionType type = defType;
    if (!TypeFromString(ReadStr(section, L"Type", L"Preset", path), &type)) return;
    if (type == ActionType::Preset) {
        PresetId pid = defPreset;
        if (!PresetFromString(ReadStr(section, L"Value",
                                      PresetToString(defPreset), path), &pid)) return;
        b->type = type;
        b->preset = pid;
    } else if (type == ActionType::Keys) {
        Hotkey hk;
        if (!HotkeyFromText(ReadStr(section, L"Value", L"", path), &hk)) return;
        b->type = type;
        b->hotkey = hk;
    } else {
        b->type = type;
    }
}

bool SaveButton(const wchar_t* section, const ButtonConfig& b,
                const std::wstring& path) {
    const wchar_t* type = L"Preset";
    std::wstring value;
    switch (b.type) {
        case ActionType::Preset: type = L"Preset"; value = PresetToString(b.preset); break;
        case ActionType::Keys: type = L"Keys"; value = HotkeyToText(b.hotkey); break;
        case ActionType::Passthrough: type = L"Passthrough"; break;
        case ActionType::Disabled: type = L"Disabled"; break;
    }
    const BOOL a = WritePrivateProfileStringW(section, L"Type", type, path.c_str());
    const BOOL c = WritePrivateProfileStringW(section, L"Value", value.c_str(), path.c_str());
    return a && c;
}

}  // namespace

void SetButtonDefault(ButtonConfig* b, bool isButton4) {
    *b = ButtonConfig{};
    b->type = ActionType::Preset;
    b->preset = isButton4 ? PresetId::Paste : PresetId::Copy;
}

void ResetConfig(Config* c) {
    *c = Config{};
    SetButtonDefault(&c->btn4, true);
    SetButtonDefault(&c->btn5, false);
}

std::wstring ConfigDirPath() {
    wchar_t buf[MAX_PATH];
    UINT n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L".";
    return std::wstring(buf) + L"\\MouseSide";
}

std::wstring ConfigFilePath() {
    return ConfigDirPath() + L"\\config.ini";
}

bool LoadConfig(Config* c) {
    ResetConfig(c);
    const std::wstring path = ConfigFilePath();
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    c->enabled = GetPrivateProfileIntW(L"General", L"Enabled", 1, path.c_str()) != 0;
    c->autostart = GetPrivateProfileIntW(L"General", L"AutoStart", 0, path.c_str()) != 0;
    LoadButton(L"Button4", &c->btn4, PresetId::Paste, path);
    LoadButton(L"Button5", &c->btn5, PresetId::Copy, path);
    return true;
}

bool SaveConfig(const Config& c) {
    CreateDirectoryW(ConfigDirPath().c_str(), nullptr);
    const std::wstring path = ConfigFilePath();
    bool ok = true;
    ok = WritePrivateProfileStringW(L"General", L"Enabled", c.enabled ? L"1" : L"0", path.c_str()) && ok;
    ok = WritePrivateProfileStringW(L"General", L"AutoStart", c.autostart ? L"1" : L"0", path.c_str()) && ok;
    ok = SaveButton(L"Button4", c.btn4, path) && ok;
    ok = SaveButton(L"Button5", c.btn5, path) && ok;
    return ok;
}

bool ApplyAutostart(bool on) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE | KEY_QUERY_VALUE, &h) != ERROR_SUCCESS)
        return false;
    bool ok;
    if (on) {
        const std::wstring cmd = L"\"" + ExePath() + L"\" /tray";
        ok = RegSetValueExW(h, L"MouseSide", 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(cmd.c_str()),
                            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)))
             == ERROR_SUCCESS;
    } else {
        const LSTATUS r = RegDeleteValueW(h, L"MouseSide");
        ok = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);  // 不存在视为已关闭
    }
    RegCloseKey(h);
    return ok;
}
