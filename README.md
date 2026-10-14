# MouseSide

Windows 轻量级鼠标侧键自定义工具。用尽可能少的代码和系统资源，把鼠标第四 / 第五按钮映射成你需要的行为。

目标是替代 X-Mouse Button Control（XMBC）——只保留日常真正在用的两个侧键映射，去掉所有用不上的高级功能。

---

## 功能

**默认行为（与 XMBC 习惯一致）**

| 鼠标按键 | 默认动作 | 快捷键 |
| --- | --- | --- |
| 第四按钮 `[4]`（XBUTTON1） | 粘贴 | `Ctrl + V` |
| 第五按钮 `[5]`（XBUTTON2） | 复制 | `Ctrl + C` |

映射生效时会拦截原始的前进 / 后退事件，浏览器不会同时跳转。

**可配置项**

- 预设动作：复制、粘贴、剪切、撤销、重做、全选、浏览器后退、浏览器前进
- 自定义快捷键：直接按下键盘组合录制（支持 `Ctrl` / `Alt` / `Shift` / `Win` 修饰键）
- 保留鼠标原始功能（不拦截）
- 禁用此侧键（吞掉点击，什么都不做）
- 一键启用 / 暂停全部映射
- 开机自动启动（当前用户级，无需管理员权限）
- 恢复默认配置

**其他**

- 常驻系统托盘：双击打开设置，右键菜单可打开设置 / 启用暂停 / 退出
- 关闭设置窗口 = 隐藏到托盘，不等于退出程序
- 单实例运行，重复打开只会唤醒已有实例的设置窗口
- 配置修改立即生效，无需重启

---

## 构建

### 环境要求

- Windows 11 x64（Windows 10 同样可用）
- Visual Studio 2022（含「使用 C++ 的桌面开发」工作负载）
- CMake 3.20 或更高

### 命令行构建

```bat
cd MouseSide
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物：`build/bin/MouseSide.exe`

使用 Ninja / NMake 时默认即为 Release：

```bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 关于 EXE 体积与依赖

CMake 中已经配置为**静态链接 CRT**（`/MT`）、`/O2 /GL /LTCG`、`/OPT:REF /OPT:ICF`。
生成的 EXE 不依赖 VC++ 运行库，双击即可运行，无需安装。

---

## 使用

1. 运行 `MouseSide.exe`，程序直接进入系统托盘并打开设置窗口。
2. 点击某个按钮行右侧的「修改」，从菜单中选择预设动作，或选择「自定义快捷键…」后按下你要的组合键，再点「保存」。
3. 关闭设置窗口不会退出程序——从托盘右键菜单选择「退出」才会真正结束。

### 配置存储

INI 文件，路径：

```
%LOCALAPPDATA%\MouseSide\config.ini
```

示例内容：

```ini
[General]
Enabled=1
AutoStart=0

[Button4]
Action=Paste
Vk=0
Mods=0

[Button5]
Action=Custom
Vk=83
Mods=5
```

- `Action` 取值：`Paste` `Copy` `Cut` `Undo` `Redo` `SelectAll` `Back` `Forward` `Custom` `KeepOriginal` `Disabled`
- `Vk` 为 Win32 虚拟键码（仅 `Custom` 有效）
- `Mods` 为修饰键位掩码：`1`=Ctrl，`2`=Alt，`4`=Shift，`8`=Win（仅 `Custom` 有效）
- 文件缺失时首次运行会自动生成默认配置；数值非法时会安全回落到默认值

### 开机自启

写入 `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` 下的 `MouseSide` 键，
命令行为 `"<程序路径>" --tray`。带 `--tray` 启动时只进托盘，不弹出设置窗口。
关闭该开关会删除对应注册表项。

---

## 项目结构

```
MouseSide/
├── CMakeLists.txt
├── README.md
├── assets/
│   ├── app.manifest        应用清单（asInvoker + PerMonitorV2 DPI + comctl32 v6）
│   ├── resources.rc        图标 / 清单 / 版本信息
│   └── MouseSide.ico       多尺寸图标（由 tools/make_icon.py 生成）
├── tools/
│   ├── make_icon.py        纯标准库生成 .ico，无需 Pillow
│   └── static_check.py     无编译器环境下的括号 / include / 标点自检
└── src/
    ├── main.cpp            入口：单实例、通用控件初始化、消息循环
    ├── App.h/.cpp          主控：配置 + 钩子 + 界面协同
    ├── MouseHook.h/.cpp    模块 1：WH_MOUSE_LL 侧键监听与拦截
    ├── KeyMapper.h/.cpp    模块 2：动作模型与 SendInput 注入
    ├── Config.h/.cpp       模块 3：INI 配置持久化
    ├── TrayUI.h/.cpp       模块 4：系统托盘 + 设置窗口
    ├── HotkeyRecorder.h/.cpp  自定义快捷键录制
    ├── IconFactory.h/.cpp  运行时绘制托盘图标（运行 / 暂停两态）
    ├── Utils.h             DPI 缩放、字体、窗口居中等小工具
    └── AppInfo.h           应用名称、版本、资源 ID
```

---

## 关键实现说明

**钩子回调保持极轻量。** `WH_MOUSE_LL` 的回调里只做三件事：判断是否该拦截、`PostMessage` 投递一个自定义消息、返回 1 吞掉事件。真正的 `SendInput` 注入发生在消息循环中。这是低延迟和钩子不被系统摘除的关键。

**热路径不做查表。** 配置变化时由 UI 线程把「是否拦截」预计算成一个 `bool intercept[2]`，钩子回调只做一次数组读取。

**自身注入的事件带标记。** 所有通过 `SendInput` 发出的输入都带 `dwExtraInfo = 'MSID'`。钩子识别到该标记就放行，因此「浏览器后退 / 前进」这类需要重放鼠标事件的动作不会形成递归拦截。

**按下与松开一并吞掉。** 只吞按下会让应用处于半按下的状态，两者的行为都可能不可预期。

**暂停 ≠ 卸载。** 暂停时钩子仍然挂着，只是关闭拦截开关，恢复时可以立即生效，也不会因为重新安装钩子失败而卡住。

**低频钩子自愈。** 界面层每 120 秒执行一次钩子重建，用于应对极端情况下系统静默摘除钩子。间隔刻意拉长以规避「持续轮询」；重建窗口只有微秒级。

**UI 全部为 Win32 原生控件。** 设置窗口用两个自绘卡片做视觉分层，背景取 `COLOR_BTNFACE` 以便与主题化复选框自然融合。没有 Electron、没有 WebView2、没有 Python / Node 运行时。

**快捷键录制用临时键盘钩子。** 录制期间吞掉所有按键，避免 `Win` 弹出开始菜单、`Alt+F4` 关窗等副作用。并且只在录制窗口处于前台时才吞键——即使出现异常，键盘也不会被长时间阻塞。

**开机自启用 `--tray` 参数区分启动来源。** 只有自启动才会静默进托盘。

---

## 已知限制

- **不拦截高权限窗口的输入。** 当目标窗口以管理员权限运行、或处于游戏、安全桌面（UAC 提示、Ctrl+Alt+Del）时，Windows 的低层钩子无法介入，映射在这些场景下不生效。程序不会尝试提权或绕过系统限制。
- **不识别鼠标品牌、型号或连接方式。** 蓝牙与 2.4GHz 都走标准 Windows 扩展鼠标事件，程序不做区分，两种连接方式行为一致。
- **需要完全退出 XMBC。** 两个程序同时拦截侧键会互相冲突。MouseSide 不会检测或卸载 XMBC。
- **自定义快捷键不能模拟 Windows 保留的安全组合键**（如 Ctrl+Alt+Del）。
- **不支持 `Fn` 等硬件专用按键**，这类按键不产生标准 Windows 键码。
- **不支持在管理员权限窗口中注入键盘事件**（UIPI 限制），此时映射动作会被系统静默丢弃。
- **开机自启只对当前用户生效**，切换用户或修改用户名后需重新设置。
- **「恢复默认配置」不会重置「开机自动启动」**，该选项属于系统层集成，需要单独关闭。

---

## V1.0 之外

以下功能明确不在首版范围内，仅在架构上预留扩展空间：

- V1.1：修饰键组合映射（`Alt` + 侧键、`Ctrl` + 侧键…）
- V1.2：应用专属映射（不同前台程序使用不同配置）
- V2.0：设备信息与电量显示（重点适配小米无线鼠标 3 彩色版）

暂不计划：宏录制、长按 / 双击映射、DPI 调节、多鼠标独立配置、云同步、插件系统、自动更新。

---

## 开发辅助脚本

```bat
:: 重新生成图标（需要 Python 3，无需第三方库）
python tools\make_icon.py

:: 在没有编译器的环境下做一次静态自检
python tools\static_check.py
```

`tools/static_check.py` 会检查括号配平、`#include` 引用是否真实存在、代码中是否混入中文标点。它**不能替代编译**，只是提交前的兜底检查。
