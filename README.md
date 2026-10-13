# MouseSide

Windows 轻量级鼠标侧键自定义工具。专注于两件事：**把鼠标第四按钮映射为粘贴、第五按钮映射为复制**，然后安静地待在系统托盘里。

C++20 + 原生 Win32 API 实现，单文件 EXE，无驱动、无运行环境依赖、无网络请求、无后台轮询。

---

## 一、功能特性

| 功能 | 说明 |
|---|---|
| 侧键识别 | 识别标准 `XBUTTON1`（第四按钮）与 `XBUTTON2`（第五按钮），蓝牙 / 2.4GHz 均适用，不需要专用鼠标驱动 |
| 默认映射 | 第四按钮 = 粘贴（Ctrl + V），第五按钮 = 复制（Ctrl + C），与 X-Mouse Button Control 的使用习惯一致 |
| 预设功能 | 复制、粘贴、剪切、撤销、重做、全选、浏览器后退、浏览器前进 |
| 自定义快捷键 | 直接按下键盘组合完成录制，支持 Ctrl / Alt / Shift / Win + 常规按键 |
| 特殊处理 | 「保留鼠标原始功能」「禁用此侧键」 |
| 一键启停 | 暂停后侧键恢复 Windows 原始行为（浏览器前进 / 后退） |
| 系统托盘 | 双击打开设置，右键菜单支持打开设置、启用 / 暂停映射、退出 |
| 配置持久化 | INI 文件保存，修改后立即生效，重启后恢复，文件损坏自动回落默认值 |
| 开机自启 | 用户级 `HKCU\...\Run` 开关，默认关闭，无需管理员权限 |
| 单实例 | 重复运行只唤醒已有实例的设置窗口，不会创建第二个监听进程 |

模拟输入采用 `SendInput` 发送目标快捷键，**不读取、不改写系统剪贴板**。

---

## 二、编译

### 2.1 MSVC（推荐，PRD 指定工具链）

环境：Visual Studio 2022（含「使用 C++ 的桌面开发」工作负载）+ CMake ≥ 3.20。

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

产物：`build\Release\MouseSide.exe`

Release 配置默认静态链接 C 运行时（`/MT`），不依赖 VC 运行库，可单独拷贝到任意目录运行。

### 2.2 MinGW-w64（可选）

```bash
cmake -S . -B build-mingw -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
cmake --build build-mingw
```

本仓库中的 `dist/MouseSide.exe` 即由该方式交叉编译产出，仅用于验证代码可构建；**正式使用请用 MSVC Release 版本**。

---

## 三、运行

1. 双击 `MouseSide.exe`，首次运行会自动创建配置文件并打开设置窗口。
2. 关闭设置窗口后程序继续驻留系统托盘；完全退出请使用托盘右键菜单的「退出程序」。
3. 需要开机自启时，在设置窗口勾选「开机自动启动」（写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，启动参数为 `--startup`，不会弹出设置窗口）。

### 命令行参数

| 参数 | 作用 |
|---|---|
| `--startup` | 开机自启时使用，静默启动到托盘，不打开设置窗口 |

---

## 四、配置文件

默认位置：`%LOCALAPPDATA%\MouseSide\config.ini`

```ini
[MouseSide]
Version=1
Enabled=1
AutoStart=0
Button4=preset:paste
Button5=preset:copy
```

| 字段 | 取值 | 说明 |
|---|---|---|
| `Enabled` | `1` / `0` | 是否启用侧键映射（0 = 暂停，侧键恢复原始行为） |
| `AutoStart` | `1` / `0` | 开机自动启动 |
| `Button4` | 见下 | 鼠标第四按钮（XBUTTON1）的映射 |
| `Button5` | 见下 | 鼠标第五按钮（XBUTTON2）的映射 |

映射取值格式：

| 格式 | 示例 | 含义 |
|---|---|---|
| `preset:<名称>` | `preset:paste` | 预设功能，名称：`copy` `paste` `cut` `undo` `redo` `selectall` `browserback` `browserforward` |
| `custom:[ctrl+][alt+][shift+][win+]<vk>` | `custom:ctrl+shift+83` | 自定义快捷键，主键用虚拟键码十进制或 `0x` 十六进制表示（83 = S） |
| `original` | `original` | 保留鼠标原始功能（不拦截） |
| `disabled` | `disabled` | 禁用此侧键（拦截但不执行任何动作） |

首次运行自动生成默认配置；文件缺失或字段非法时，对应项自动回落为默认值（Button4 = 粘贴，Button5 = 复制）。

---

## 五、目录结构

```
MouseSide/
├── CMakeLists.txt
├── README.md
├── res/                  图标资源（正常态 / 暂停态）
└── src/
    ├── main.cpp          入口：单实例、消息循环、隐藏主窗口、配置应用
    ├── Common.h          公共类型、常量、自定义消息
    ├── Config.*          INI 读写、默认值、开机自启
    ├── MouseHook.*       WH_MOUSE_LL 钩子，识别与拦截 XBUTTON1/2
    ├── KeyMapper.*       映射执行（SendInput）与显示文本
    ├── TrayUI.*          系统托盘图标与右键菜单
    ├── SettingsWindow.*  设置窗口与快捷键录制窗口
    ├── app.rc / app.manifest / resource.h
```

模块对应关系与 PRD 一致：`MouseHook` → `KeyMapper` → `Config` / `TrayUI`。

关键实现约定：

* 钩子回调只做判断并 `PostMessage`，模拟输入在窗口过程中执行，钩子回调保持轻量；
* 侧键按下被拦截后，对应的松开 / 双击事件同样拦截，避免浏览器收到「孤儿」事件与重复触发；
* 暂停或映射为「保留原始功能」时，事件完全放行；
* 退出时依次释放托盘图标与鼠标钩子，进程异常结束也不会导致侧键永久失效（Windows 会自动移除钩子）。

---

## 六、使用限制

* **管理员权限窗口**：程序以标准权限运行时，无法向以管理员身份运行的窗口发送模拟输入（UIPI 限制）。本程序不通过提权绕过系统限制，有需要请以管理员身份运行 MouseSide。
* **安全桌面与系统安全快捷键**：UAC 提示、锁屏、Ctrl + Alt + Del 等场景下的输入不受支持，也不尝试模拟。
* **部分游戏 / 全屏独占程序**：可能直接读取原始输入（Raw Input）或使用反作弊驱动，侧键映射可能不生效。
* **与 X-Mouse Button Control 冲突**：MouseSide 用于替代 XMBC，请勿让两者同时管理相同侧键，测试前请完全退出 XMBC。
* **映射粒度**：V1.0 仅支持侧键直接点击的映射，不区分应用、不支持修饰键条件组合、不支持长按 / 双击 / 宏。

---

## 七、V1.0 验收清单

基础功能

- [ ] 第四按钮默认执行 Ctrl + V，第五按钮默认执行 Ctrl + C
- [ ] 每次单击只触发一次目标操作，不同时触发浏览器前进 / 后退
- [ ] 两个侧键可分别修改，自定义快捷键保存并立即生效
- [ ] 支持「保留原始功能」与「禁用此侧键」
- [ ] 暂停映射 / 退出程序后鼠标恢复原始行为

系统集成

- [ ] 托盘图标正常显示，可打开设置、暂停、退出（暂停态图标区分）
- [ ] 关闭设置窗口后仍后台运行
- [ ] 配置重启后恢复，开机自启可开启与关闭
- [ ] 重复启动只唤醒已有实例
- [ ] 不需要安装鼠标驱动，独立 EXE 直接运行

兼容性

- [ ] 蓝牙模式与 2.4GHz 接收器模式下均正常
- [ ] 鼠标断开重连、系统睡眠唤醒后仍可使用
- [ ] 浏览器、记事本、资源管理器、代码编辑器等常规程序响应正常
- [ ] 快速连续点击不出现异常重复触发

性能

- [ ] 空闲 CPU 接近 0%（消息驱动，无轮询）
- [ ] 常驻内存与 EXE 体积保持低水平
- [ ] 长时间后台运行无明显内存增长
- [ ] 无网络请求、无后台服务

---

## 八、后续版本规划

* **V1.1**：修饰键组合映射（Alt / Ctrl / Shift + 侧键）
* **V1.2**：应用专属映射（按前台程序切换规则）
* **V2.0**：设备信息与电量显示（视设备暴露的 Windows / 蓝牙接口而定）

代码结构上，`Mapping` 与配置序列化已为组合映射和应用规则预留扩展位，V1.0 不做过度设计。
