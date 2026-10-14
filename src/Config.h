// Config.h
//
// 模块 3 / 4：配置持久化
//
//   - 配置以 INI 形式保存在 %LOCALAPPDATA%\MouseSide\config.ini
//   - 不依赖网络、不依赖数据库
//   - 首次运行自动建立默认配置
//   - 配置缺失或损坏时安全回落到默认值

#pragma once

#include <windows.h>

#include <string>

#include "KeyMapper.h"

namespace ms {

// 支持映射的侧键数量：0 = 鼠标第四按钮（XBUTTON1），1 = 鼠标第五按钮（XBUTTON2）
constexpr int kButtonCount = 2;

struct AppConfig {
    bool         enabled   = true;   // 映射是否启用（false = 暂停，放行原始侧键）
    bool         autoStart = false;  // 是否随 Windows 登录自动启动
    MappedAction buttons[kButtonCount];
};

namespace Config {

// %LOCALAPPDATA%\MouseSide\config.ini
std::wstring DefaultPath();

// 出厂默认配置：第四按钮 = 粘贴，第五按钮 = 复制
AppConfig MakeDefault();

// 读取配置。文件不存在时会写入一份默认配置再返回。
AppConfig Load(const std::wstring& path);

// 写入配置。目录不存在时自动创建。
bool Save(const std::wstring& path, const AppConfig& config);

// 修正非法值（未知动作名、越界虚拟键、非法修饰位等）
void Sanitize(AppConfig& config);

// 判断两个配置在「生效层面」是否等价（用于避免无意义的写盘）
bool SameAs(const AppConfig& lhs, const AppConfig& rhs);

// 配置文件的完整路径是否可写（用于启动时提示）
bool EnsureDirectory(const std::wstring& filePath);

}  // namespace Config
}  // namespace ms
