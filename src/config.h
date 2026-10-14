#pragma once
// config —— 配置模型与 INI 读写。单线程使用（全部运行在 UI 线程）。
#include <string>
#include "vkmap.h"

enum class ActionType {
    Preset,       // 预设功能
    Keys,         // 自定义键盘快捷键
    Passthrough,  // 保留鼠标原始功能
    Disabled,     // 禁用此侧键
};

enum class PresetId {
    Copy, Paste, Cut, Undo, Redo, SelectAll, BrowserBack, BrowserForward,
};

struct ButtonConfig {
    ActionType type = ActionType::Preset;
    PresetId preset = PresetId::Paste;
    Hotkey hotkey;
};

struct Config {
    bool enabled = true;
    bool autostart = false;
    ButtonConfig btn4;  // XBUTTON1 / 第四按钮
    ButtonConfig btn5;  // XBUTTON2 / 第五按钮
};

void SetButtonDefault(ButtonConfig* b, bool isButton4);
void ResetConfig(Config* c);          // btn4=粘贴, btn5=复制, enabled=1, autostart=0

std::wstring ConfigDirPath();         // %LOCALAPPDATA%\MouseSide
std::wstring ConfigFilePath();        // %LOCALAPPDATA%\MouseSide\config.ini

// 返回 false 表示首次运行（文件不存在），c 保持默认值；损坏字段回退默认值。
bool LoadConfig(Config* c);
bool SaveConfig(const Config& c);  // false = INI 写入失败（磁盘/权限）

// 开机自启：HKCU\...\Run 写/删 "MouseSide"（带 /tray 参数，启动后仅驻留托盘）
bool ApplyAutostart(bool on);      // false = 注册表操作失败
