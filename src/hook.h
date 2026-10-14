#pragma once
// hook —— MouseHook：低级鼠标钩子，仅监听 XBUTTON1 / XBUTTON2。
namespace hook {

bool Install();     // false = 安装失败（被其他程序占用等）
void Uninstall();
bool Installed();

}  // namespace hook
