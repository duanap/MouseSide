#include "Config.h"
#include <shlobj.h>
#include <vector>
namespace ms {
std::wstring systemError(DWORD code) {
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring text = message ? message : L"Windows 错误 " + std::to_wstring(code);
    if (message) LocalFree(message);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) text.pop_back();
    return text;
}
std::filesystem::path ConfigStore::defaultPath() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &folder)))
        throw std::runtime_error("Cannot locate LOCALAPPDATA");
    std::filesystem::path result = std::filesystem::path(folder) / L"MouseSide" / L"config.ini";
    CoTaskMemFree(folder);
    return result;
}
Config ConfigStore::load(bool& repaired) const {
    Config config;
    repaired = false;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path_, ec);
    if (ec || size == 0 || size > 65536) { repaired = true; return config; }
    auto read = [&](const wchar_t* key) {
        wchar_t value[128]{};
        const DWORD length = GetPrivateProfileStringW(L"MouseSide", key, L"", value, 128, path_.c_str());
        if (length == 127) return std::wstring();
        return std::wstring(value);
    };
    for (unsigned i = 0; i < 2; ++i) {
        auto parsed = decodeAction(read(i == 0 ? L"Button4" : L"Button5"));
        if (parsed) config.buttons[i] = *parsed; else repaired = true;
    }
    auto boolean = [&](const wchar_t* key, bool& target) {
        const std::wstring value = read(key);
        if (value == L"1") target = true;
        else if (value == L"0") target = false;
        else repaired = true;
    };
    boolean(L"Enabled", config.enabled);
    boolean(L"AutoStart", config.autostart);
    return config;
}
bool ConfigStore::save(const Config& config, std::wstring& error) const {
    for (const Action& action : config.buttons)
        if (action.kind == ActionKind::Shortcut && !validShortcut(action.shortcut)) { error = L"快捷键配置无效。"; return false; }
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) { error = L"无法创建配置目录。"; return false; }
    const auto temp = std::filesystem::path(path_.wstring() + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp");
    const std::wstring text = L"\xFEFF[MouseSide]\r\nButton4=" + encodeAction(config.buttons[0]) +
        L"\r\nButton5=" + encodeAction(config.buttons[1]) + L"\r\nEnabled=" + (config.enabled ? L"1" : L"0") +
        L"\r\nAutoStart=" + (config.autostart ? L"1" : L"0") + L"\r\n";
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = L"无法保存配置：" + systemError(GetLastError()); return false; }
    DWORD written = 0;
    const auto bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    bool ok = WriteFile(file, text.data(), bytes, &written, nullptr) && written == bytes && FlushFileBuffers(file);
    DWORD code = GetLastError();
    CloseHandle(file);
    if (ok) { ok = MoveFileExW(temp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE; code = GetLastError(); }
    if (!ok) { DeleteFileW(temp.c_str()); error = L"无法保存配置：" + systemError(code); }
    else error.clear();
    return ok;
}
static std::wstring startupCommand() {
    std::vector<wchar_t> buffer(32768);
    DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (n == 0 || n >= buffer.size()) return {};
    return L"\"" + std::wstring(buffer.data(), n) + L"\" --background";
}
constexpr const wchar_t* runPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* runName = L"MouseSideCodex";
bool readAutostart() {
    wchar_t buffer[32768]{};
    DWORD bytes = sizeof(buffer);
    if (RegGetValueW(HKEY_CURRENT_USER, runPath, runName, RRF_RT_REG_SZ, nullptr, buffer, &bytes) != ERROR_SUCCESS) return false;
    const auto command = startupCommand();
    return !command.empty() && _wcsicmp(command.c_str(), buffer) == 0;
}
bool writeAutostart(bool enabled, std::wstring& error) {
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, runPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) { error = L"无法修改自启设置：" + systemError(static_cast<DWORD>(result)); return false; }
    if (enabled) {
        const auto command = startupCommand();
        if (command.empty()) result = ERROR_BAD_PATHNAME;
        else result = RegSetValueExW(key, runName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, runName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) error = L"无法修改自启设置：" + systemError(static_cast<DWORD>(result));
    return result == ERROR_SUCCESS;
}
}
