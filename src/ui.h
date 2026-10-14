#pragma once
// ui —— TrayUI 的设置窗口与映射对话框。
namespace ui {

bool CreateAll();  // 创建设置窗口（隐藏）；失败返回 false
void Show();       // 显示并前置设置窗口
void Refresh();    // 配置变化后同步控件文本与勾选状态

}  // namespace ui
