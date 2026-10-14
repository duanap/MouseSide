// MouseSide - 配置读写（INI，位于 %LOCALAPPDATA%\MouseSide\config.ini）
#pragma once

#include "Common.h"

namespace ms {

struct AppConfig;

namespace config {

// 配置文件完整路径
std::wstring Path();

// 加载配置。缺失/非法字段回落默认值；文件不存在时尝试写入默认配置。
// 返回值：true = 配置文件原先不存在（首次运行）
bool Load(AppConfig& cfg);

// 完整写入同目录临时文件后替换原文件；失败时保留原配置并清理临时文件。
bool Save(const AppConfig& cfg);

// 恢复全部默认配置；是否保留 autoStart 由调用方决定。
void ResetToDefault(AppConfig& cfg);

// 开机自启（HKCU Run 键，用户级，无需管理员权限）
bool GetAutoStart();
bool SetAutoStart(bool enable);

}  // namespace config
}  // namespace ms
