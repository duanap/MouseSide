# MouseSide Implementation Plan

> 执行方式：本会话使用 executing-plans 实现，最终代码审查及本机验证。

**Goal:** 按 PRD 从零交付 Windows 双侧键映射工具。
**Architecture:** 可测试 Model / Config + 独立输入消息线程 + 原生设置与托盘。
**Tech Stack:** C++20、Win32、CMake；优先 MSVC，可用工具链构建也保留 MSVC 支持。
**Spec:** ../specs/2026-10-14-mouseside-design.md

## Global Constraints

Windows 11 x64；独立 EXE；无浏览器内核、无网络、无额外服务；默认第四键粘贴、第五键复制；零现有实现复用。

## Review Focus

1. 暂停/修改发生在按下与松开之间：事件必须成对处理。
2. 用户持有左右修饰键：快捷键准确执行且不留下卡键。
3. 录制 Win+D / Alt+Tab 和失焦：不会触发目标操作或留下录制钩子。
4. 配置损坏与写入失败：安全默认、错误反馈、保留旧配置。
5. Explorer 重启、重复打开、输入初始化失败：保留可见控制入口。

## Task 1: Model / Config

Files: CMakeLists.txt, src/Model.*, src/Config.*, tests/core_tests.cpp。
Interfaces: Action、Shortcut、Config、ButtonState；ConfigStore::load/save。
- [ ] 写默认值、无效键码、损坏字段、点击去重、暂停中途切换、配置往返的失败测试。
- [ ] 运行测试确认失败，然后实现并通过 CTest。

## Task 2: Input

Files: src/Input.*, tests/input_tests.cpp。
Interfaces: InputEngine::start/update/stop；BuildKeySequence；ShortcutRecorder。
- [ ] 编写键盘序列及真实 Win32 接收器集成测试，验证失败。
- [ ] 实现钩子线程、消息投递、SendInput、按键清理和录制。
- [ ] 运行 CTest；测试隔离窗口和配置，自动清理。

## Task 3: Application / UI

Files: src/App.*, src/main.cpp, res/app.rc, res/app.manifest, scripts/*。
Interfaces: App::run，配置变化后持久化并更新输入线程。
- [ ] 编写独立 GUI 生命周期测试（后台启动、重复启动、关闭隐藏、退出）。
- [ ] 实现原生卡片界面、DPI、托盘、自启、唤醒、错误消息。
- [ ] 编译 Release，运行测试，截图检查视觉，测量体积/内存/空闲 CPU。

## Task 4: Review / Delivery

Files: README.md, config.default.ini, docs/verification.md, dist/MouseSide.exe。
- [ ] 审查 P0/P1 覆盖和故障路径，修复重要发现并重跑测试。
- [ ] 写构建和使用说明、配置格式、已验证与待人工验收项。
- [ ] 提交 codex 分支，交付 EXE，验证 Git 历史独立及工作区状态。
