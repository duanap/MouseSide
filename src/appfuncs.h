#pragma once
// 跨模块动作接口（实现在 main.cpp）
namespace app {

void SetEnabled(bool on);   // 启用/暂停映射并同步托盘与界面
void ShowSettings();
void ExitApp();

}  // namespace app
