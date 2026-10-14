// Config.cpp

#include "Config.h"

#include <iterator>
#include <string>

namespace ms {
namespace {

constexpr wchar_t kSectionGeneral[] = L"General";
constexpr wchar_t kSectionButton4[] = L"Button4";  // 第四按钮
constexpr wchar_t kSectionButton5[] = L"Button5";  // 第五按钮

struct ButtonSections {
    const wchar_t* section;
    ActionType     fallback;
};

const ButtonSections kButtonSections[kButtonCount] = {
    {kSectionButton4, ActionType::Paste},
    {kSectionButton5, ActionType::Copy},
};

std::wstring ReadString(const std::wstring& path,
                        const wchar_t* section,
                        const wchar_t* key,
                        const wchar_t* fallback) {
    wchar_t buffer[128] = {};
    GetPrivateProfileStringW(section, key, fallback, buffer,
                             static_cast<DWORD>(std::size(buffer)), path.c_str());
    return std::wstring(buffer);
}

int ReadInt(const std::wstring& path,
            const wchar_t* section,
            const wchar_t* key,
            int fallback,
            int minValue,
            int maxValue) {
    int value = static_cast<int>(
        GetPrivateProfileIntW(section, key, fallback, path.c_str()));
    if (value < minValue) {
        value = minValue;
    }
    if (value > maxValue) {
        value = maxValue;
    }
    return value;
}

void WriteInt(const std::wstring& path,
              const wchar_t* section,
              const wchar_t* key,
              int value) {
    WritePrivateProfileStringW(section, key, std::to_wstring(value).c_str(), path.c_str());
}

std::wstring DirectoryOf(const std::wstring& filePath) {
    const size_t pos = filePath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return std::wstring();
    }
    return filePath.substr(0, pos);
}

bool EnsureDirectoryRecursive(const std::wstring& directory) {
    if (directory.empty()) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return true;
    }
    // 只创建一级目录已经足够（LOCALAPPDATA 一定存在），但仍逐级兜底
    std::wstring partial;
    for (size_t i = 0; i <= directory.size(); ++i) {
        if (i == directory.size() || directory[i] == L'\\' || directory[i] == L'/') {
            if (partial.size() > 2) {  // 跳过 "C:" 这类根前缀
                CreateDirectoryW(partial.c_str(), nullptr);
            }
        }
        if (i < directory.size()) {
            partial.push_back(directory[i]);
        }
    }
    const DWORD check = GetFileAttributesW(directory.c_str());
    return check != INVALID_FILE_ATTRIBUTES && (check & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

}  // namespace

namespace Config {

std::wstring DefaultPath() {
    wchar_t buffer[MAX_PATH] = {};

    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer,
                                                 static_cast<DWORD>(std::size(buffer)));
    if (length > 0 && length < std::size(buffer)) {
        return std::wstring(buffer) + L"\\MouseSide\\config.ini";
    }

    // 极端环境兜底：与 EXE 同目录
    const DWORD exeLength = GetModuleFileNameW(nullptr, buffer,
                                               static_cast<DWORD>(std::size(buffer)));
    if (exeLength > 0 && exeLength < std::size(buffer)) {
        return DirectoryOf(buffer) + L"\\MouseSide.ini";
    }
    return L"MouseSide.ini";
}

AppConfig MakeDefault() {
    AppConfig config;
    config.enabled   = true;
    config.autoStart = false;

    config.buttons[0].type = ActionType::Paste;  // 第四按钮 -> Ctrl + V
    config.buttons[0].vk   = 0;
    config.buttons[0].mods = 0;

    config.buttons[1].type = ActionType::Copy;   // 第五按钮 -> Ctrl + C
    config.buttons[1].vk   = 0;
    config.buttons[1].mods = 0;

    return config;
}

void Sanitize(AppConfig& config) {
    for (int i = 0; i < kButtonCount; ++i) {
        MappedAction& button = config.buttons[i];

        button.mods &= (MOD_CTRL | MOD_ALT | MOD_SHIFT | MOD_WIN);

        if (button.vk > 0xFFu) {
            button.vk = 0;
        }

        if (button.type == ActionType::Custom && button.vk == 0) {
            // 自定义动作缺少有效按键 -> 回落到该按钮的出厂默认
            button.type = kButtonSections[i].fallback;
            button.mods = 0;
        }
        if (button.type != ActionType::Custom) {
            button.vk   = 0;
            button.mods = 0;
        }
    }
}

AppConfig Load(const std::wstring& path) {
    AppConfig config = MakeDefault();

    if (!FileExists(path)) {
        // 首次运行：建立默认配置
        Save(path, config);
        return config;
    }

    config.enabled   = ReadInt(path, kSectionGeneral, L"Enabled", 1, 0, 1) != 0;
    config.autoStart = ReadInt(path, kSectionGeneral, L"AutoStart", 0, 0, 1) != 0;

    for (int i = 0; i < kButtonCount; ++i) {
        const wchar_t* section = kButtonSections[i].section;
        MappedAction&  button  = config.buttons[i];

        const std::wstring actionName =
            ReadString(path, section, L"Action",
                       ActionTypeName(kButtonSections[i].fallback));
        button.type = ActionTypeFromName(actionName, kButtonSections[i].fallback);

        button.vk   = static_cast<UINT>(ReadInt(path, section, L"Vk", 0, 0, 0xFF));
        button.mods = static_cast<UINT>(ReadInt(path, section, L"Mods", 0, 0, 0xF));
    }

    Sanitize(config);
    return config;
}

bool Save(const std::wstring& path, const AppConfig& config) {
    if (!EnsureDirectory(path)) {
        return false;
    }

    bool ok = true;
    auto writeString = [&](const wchar_t* section, const wchar_t* key, const wchar_t* value) {
        if (WritePrivateProfileStringW(section, key, value, path.c_str()) == FALSE) {
            ok = false;
        }
    };

    writeString(kSectionGeneral, L"Enabled",   config.enabled ? L"1" : L"0");
    writeString(kSectionGeneral, L"AutoStart", config.autoStart ? L"1" : L"0");

    for (int i = 0; i < kButtonCount; ++i) {
        const wchar_t* section = kButtonSections[i].section;
        const MappedAction& button = config.buttons[i];

        writeString(section, L"Action", ActionTypeName(button.type));
        WriteInt(path, section, L"Vk", static_cast<int>(button.vk));
        WriteInt(path, section, L"Mods", static_cast<int>(button.mods));
    }

    return ok;
}

bool SameAs(const AppConfig& lhs, const AppConfig& rhs) {
    if (lhs.enabled != rhs.enabled || lhs.autoStart != rhs.autoStart) {
        return false;
    }
    for (int i = 0; i < kButtonCount; ++i) {
        if (lhs.buttons[i].type != rhs.buttons[i].type ||
            lhs.buttons[i].vk != rhs.buttons[i].vk ||
            lhs.buttons[i].mods != rhs.buttons[i].mods) {
            return false;
        }
    }
    return true;
}

bool EnsureDirectory(const std::wstring& filePath) {
    return EnsureDirectoryRecursive(DirectoryOf(filePath));
}

}  // namespace Config
}  // namespace ms
