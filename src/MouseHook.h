// MouseSide - 低级鼠标钩子：识别并处理 XBUTTON1 / XBUTTON2
#pragma once

#include "Common.h"

namespace ms {
namespace mousehook {

// 安装 WH_MOUSE_LL 钩子。target 用于接收 WM_MS_EXEC_MAPPING 消息。
bool Install(HWND target);
void Uninstall();
bool Installed();

// 是否启用映射（关闭时完全放行原始侧键行为）
void SetEnabled(bool enabled);
// 更新两侧键的映射（下标 0 = XBUTTON1，1 = XBUTTON2）
void SetMappings(const Mapping& m4, const Mapping& m5);

}  // namespace mousehook
}  // namespace ms
