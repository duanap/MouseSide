# MouseSide · Codex

Windows 11 x64 原生鼠标侧键映射工具。此版本依据同一 PRD 独立开发，`codex` 分支拥有独立根历史，未读取或复用其他分支的源码、界面和图标。

## 使用

直接运行 `dist/MouseSide.exe`，无需安装、驱动或额外运行环境。测试实际鼠标前，请完全退出 XMBC 和其他版本的 MouseSide，确保只有一个程序管理侧键。

- 第四按钮 / XBUTTON1：默认 **Ctrl + V（粘贴）**。
- 第五按钮 / XBUTTON2：默认 **Ctrl + C（复制）**。
- 下拉框选择复制、粘贴、剪切、撤销、重做、全选、浏览器前进/后退、保留原始功能或禁用，立即保存并生效。
- 点击「录制快捷键」，按下组合键并松开全部按键即可保存。支持 Ctrl / Alt / Shift / Win 和常规键盘按键；Esc 取消，切换到其他窗口也取消。录制期间临时暂停侧键映射，组合键不会被发送到目标应用。
- 取消「启用侧键映射」恢复 Windows 原始侧键功能。
- 关闭或最小化设置窗口，继续驻留托盘。双击托盘打开设置，右键可暂停/启用和退出。
- 默认不开机自启；开启后，登录 Windows 时直接进入托盘。移动 EXE 后请重新关闭并开启自启，使路径更新。
- 再次打开 EXE 会唤醒已经运行的本版本；不同模型版本的单实例保护互相独立，因此请手动退出旧版本。

没有网络请求、遥测、更新检查、后台扫描或额外服务。只有点击事件触发输入处理。

## 配置

默认位置：`%LOCALAPPDATA%\MouseSide\config.ini`。首次运行创建默认配置，无效字段恢复该字段默认值。更改通过同目录临时文件与原子替换保存；失败会提示，保留旧配置。

```ini
[MouseSide]
Button4=shortcut:2:86
Button5=shortcut:2:67
Enabled=1
AutoStart=0
```

`Button4` / `Button5` 支持 `original`、`disabled` 或 `shortcut:<修饰键掩码>:<十进制 Windows VK 键码>`。修饰键位：Shift=1、Ctrl=2、Alt=4、Win=8，可以相加。例如 Ctrl+Shift+S 为 `shortcut:3:83`。推荐在界面录制，避免手写无效键码。

`Enabled`、`AutoStart` 使用 `0` 或 `1`。自启实际状态以当前用户注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 下 `MouseSideCodex` 为准；仅在用户切换自启或恢复默认时修改该值。不会修改其他版本的自启项。

不同版本按 PRD 共用配置位置，但可能使用不同格式；切换版本后请核对映射。

## 本地编译

推荐 Windows 上安装 Visual Studio C++ 桌面开发工具及 CMake，使用 MSVC 静态运行库：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

EXE 位于 `build/Release/MouseSide.exe`。也可运行 `scripts/build.ps1`，它会构建、测试并复制至 `dist`。

本次交付机没有 MSVC，交付 EXE 使用 WSL 中 MinGW-w64 GCC 14 的 Windows x64 Release 交叉构建，并在真实 Windows 桌面执行测试。构建只需要开发工具，运行 EXE 不依赖 WSL 或 MinGW DLL。MSVC 配置已提供，尚未在本机实编。

```powershell
wsl -d Debian -- bash -lc 'cd /mnt/v/MouseSide && cmake -S . -B build-codex -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-codex -j 4'
./scripts/test.ps1 -BuildDirectory ./build-codex
```

源码中的图标已经生成并提交，无需安装 Python。只有重新生成图标时，`scripts/make_icons.py` 需要 Python / Pillow。

## 使用限制与验证

- 普通权限运行。Windows UIPI 可能禁止向高权限程序注入快捷键；安全桌面、保护快捷键和特殊游戏不保证支持，不自动提权。发送失败会暂停映射并提示；恢复使用普通窗口后可重新启用。
- 不支持 Fn、宏、长按、双击、应用专属映射、多鼠标独立配置或设备电量。
- 按住多余修饰键时，会短暂释放这些键，发送准确配置的组合键后恢复；不会把已有 Shift 自动添加到 Ctrl+V。
- 低级钩子运行于独立消息线程；回调只判断并投递消息，不读写配置或调用耗时 UI。睡眠唤醒通过系统消息重新安装钩子，Explorer 重启后重新注册托盘。
- 自动测试只注入到隔离测试窗口，并对测试侧键安装隔离接收钩子，避免被其他映射程序处理。GUI 测试会备份并恢复已有配置，不修改开机自启。

详细测试、实测资源以及仍需实际鼠标验收的项目见 [docs/verification.md](docs/verification.md)。

## 源码结构

`Model`：映射、快捷键校验、点击/录制状态机。`Config`：INI 与用户级自启。`Input`：鼠标钩子线程、输入序列、录制钩子。`App`：原生控件、绘制、托盘和生命周期。
