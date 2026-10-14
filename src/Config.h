#pragma once
#include "Model.h"
#include <filesystem>
namespace ms {
class ConfigStore {
public:
    explicit ConfigStore(std::filesystem::path path) : path_(std::move(path)) {}
    static std::filesystem::path defaultPath();
    Config load(bool& repaired) const;
    bool save(const Config& config, std::wstring& error) const;
    const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};
std::wstring systemError(DWORD code);
bool readAutostart();
bool writeAutostart(bool enabled, std::wstring& error);
}
