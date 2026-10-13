// MouseSide - 设置窗口（主界面）
#pragma once

#include "Common.h"

namespace ms {
namespace settings {

// 注册窗口类
bool Register(HINSTANCE hInst);
// 打开（或激活）设置窗口；owner 为隐藏主窗口
void Show(HWND owner);
// 配置在其它位置被修改后刷新界面
void RefreshFromConfig();
// 程序退出前关闭
void CloseAll();

}  // namespace settings
}  // namespace ms
