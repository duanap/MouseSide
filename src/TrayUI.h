// MouseSide - 系统托盘图标与右键菜单
#pragma once

#include "Common.h"

namespace ms {
namespace tray {

// 添加托盘图标。hwnd 接收 WM_MS_TRAY 消息。
bool Add(HWND hwnd, HINSTANCE hInst);
void Remove();
// 根据映射启用状态切换图标与提示文本
void Update(bool mappingEnabled);
// 在光标位置弹出右键菜单
void ShowMenu(HWND hwnd);

}  // namespace tray
}  // namespace ms
