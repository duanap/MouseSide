// MouseSide - 配置读写（INI，位于 %LOCALAPPDATA%\MouseSide\config.ini）
#pragma once

#include "Common.h"

namespace ms {

struct AppConfig;

namespace config {

// 配置文件完整路径
std::wstring Path();

// 加载配置。文件不存在或内容损坏时写入默认值并返回 true（表示首次初始化）。
// 返回值：true = 配置文件原先不存在（首次运行）
bool Load(AppConfig& cfg);

// 立即保存配置（不存在时自动创建目录与文件）
bool Save(const AppConfig& cfg);

// 恢复默认配置（不改动 autoStart 之外的项由调用方决定）
void ResetToDefault(AppConfig& cfg);

// 开机自启（HKCU Run 键，用户级，无需管理员权限）
bool GetAutoStart();
bool SetAutoStart(bool enable);

}  // namespace config
}  // namespace ms
