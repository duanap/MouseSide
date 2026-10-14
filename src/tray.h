#pragma once
// tray —— TrayUI 托盘部分：图标、提示、右键菜单。
#include <windows.h>

namespace tray {

bool Init();         // 添加托盘图标（按当前启用状态选择图标）；explorer 重启后可重入
void UpdateIcon();   // 启用/暂停状态切换图标与提示
void Remove();
void OnCallback(LPARAM lParam);  // 处理托盘回调鼠标消息

}  // namespace tray
