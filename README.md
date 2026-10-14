# MouseSide

Windows 轻量级鼠标侧键自定义工具。把鼠标第四 / 第五侧键（XBUTTON1 / XBUTTON2）
映射为复制、粘贴或任意键盘快捷键，替代 X-Mouse Button Control 等重型工具。

- 原生 Win32 实现，无 .NET / Electron / WebView2 依赖
- 全局低级鼠标钩子（`WH_MOUSE_LL`）+ `SendInput` 模拟快捷键，额外延迟极低
- 单进程常驻系统托盘，空闲 CPU 接近 0%
- INI 配置本地存储，无网络请求、无遥测

## 使用

1. 运行 `MouseSide.exe`（免安装）。首次运行会显示设置窗口并生成默认配置。
2. 默认映射与 XMBC 习惯一致：
   - **第四按钮 [4] → 粘贴（Ctrl+V）**
   - **第五按钮 [5] → 复制（Ctrl+C）**
3. 双击托盘图标打开设置；右键托盘图标可打开设置、暂停映射或退出。
4. 关闭设置窗口只是隐藏到托盘，映射继续生效。
5. 重复运行 EXE 不会创建第二个监听进程，只会唤醒已有实例的设置窗口。
   命令行参数 `/tray` 用于开机自启场景：启动后直接驻留托盘、不弹窗口。

### 映射选项

每个侧键可独立配置：

| 选项 | 说明 |
|---|---|
| 预设功能 | 复制 / 粘贴 / 剪切 / 撤销 / 重做 / 全选 / 浏览器后退 / 浏览器前进 |
| 自定义快捷键 | 点击输入框后直接按下组合键录制（支持 Ctrl / Shift / Alt / Win），Esc 取消 |
| 保留原始功能 | 不拦截，恢复 Windows 默认的前进 / 后退 |
| 禁用此侧键 | 拦截并忽略，完全不响应 |

修改立即保存并即时生效，无需重启程序。

## 构建要求与步骤

技术栈：C++20 / Win32 API / CMake，Windows 11 x64（Win10 亦可）。

### MSVC（Visual Studio 或 Build Tools）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物：`build\Release\MouseSide.exe`

### MinGW-w64（GCC）

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

MinGW 构建静态链接运行库，产出免依赖的独立 EXE。

诊断构建（钩子拦截与 SendInput 日志输出到 `%TEMP%\mouseside_debug.log`）：

```bash
cmake -B build-dbg -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DMSIDE_DEBUG=ON
cmake --build build-dbg -j
```

## 测试脚本（tools/）

- `e2e_test.ps1` —— 注入 XBUTTON 事件验证复制/粘贴映射（含直接 SendInput 对照组）
- `e2e_disabled_test.ps1` —— 验证"禁用侧键"时事件被完整吞掉
- `screenshot.ps1` / `tray_shot.ps1` —— 设置窗口 / 托盘区截图
- `diag_windows.ps1` —— 窗口状态与托盘消息接线诊断
- `gen_icon.ps1` —— 重新生成 assets/ 下两个图标

## 配置文件

位置：`%LOCALAPPDATA%\MouseSide\config.ini`，首次运行自动创建，修改后立即保存。
文件缺失或字段损坏时自动回退默认值。

```ini
[General]
Enabled=1          ; 1=启用映射 0=暂停（暂停时侧键恢复原始前进/后退）
AutoStart=0        ; 开机自启开关（同时写入 HKCU\...\Run，带 /tray 参数静默启动）

[Button4]          ; XBUTTON1 / 第四按钮
Type=Preset        ; Preset | Keys | Passthrough | Disabled
Value=Paste        ; Preset: Copy/Paste/Cut/Undo/Redo/SelectAll/BrowserBack/BrowserForward
                   ; Keys: 快捷键串，如 Ctrl+Shift+S、Alt+Tab、F5、MediaNext

[Button5]          ; XBUTTON2 / 第五按钮
Type=Preset
Value=Copy
```

配置目录不含网络、账户或其他无关数据；删除该目录即可完全重置。

## 已知限制

- 映射通过模拟键盘快捷键实现，因此在**管理员权限窗口**和 **UAC 安全桌面**
  中不生效（Windows UIPI 限制，程序不会提权绕过）。
- 若录制快捷键时用户正按住其他修饰键，程序会在发送前临时释放冲突修饰键并随后恢复。
- 与其他全局鼠标工具（如 XMBC）同时运行会争抢侧键；请先完全退出对方再使用。
- 蓝牙与 2.4GHz 模式均走标准 Windows 鼠标事件，无需驱动；程序异常退出时
  系统会自动移除钩子，不会造成侧键永久失效。

## 源码结构

```
src/vkmap.cpp    虚拟键码与名称互转（配置序列化 / 界面显示）
src/config.cpp   配置模型、INI 读写、开机自启（HKCU Run）
src/mapper.cpp   KeyMapper：映射执行、SendInput 组合键、预设表
src/hook.cpp     MouseHook：WH_MOUSE_LL 钩子，仅处理 XBUTTON1/2
src/tray.cpp     TrayUI：托盘图标与右键菜单
src/ui.cpp       TrayUI：设置窗口、映射对话框、快捷键录制
src/main.cpp     宿主窗口、单实例互斥、程序入口
```

V1.0 明确不包含：分应用映射、修饰键+侧键组合、宏、DPI/电量等设备功能、
云同步、自动更新。相关规划见产品需求文档 V1.1 / V1.2 / V2.0。
