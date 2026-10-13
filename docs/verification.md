# MouseSide v1.0.1 修复验证

日期：2026-10-13。

本次修复保留 C++20 / Win32 架构，覆盖托盘消息解码和恢复、录制状态与取消、已按住按键的输入状态保护、发送失败恢复、配置保存错误提示，以及设置窗口独立退出入口。

## 实际执行

- 原始 WorkBuddy 快照：`0652eab`。同一回归测试的最初 15 个用例中，14 个失败、1 个通过，复现托盘、录制、输入状态和保存错误处理问题。
- Windows x64 Release 构建：成功。使用 Debian WSL 的 MinGW-w64 GCC 14 和 CMake 3.31.6，静态链接 C++ 运行库。
- 最终 `MouseSideRegression.exe`：29 个用例通过，0 个失败。
- `ctest --test-dir build-fixed --output-on-failure`：1/1 测试目标通过，包含上述 29 个用例。
- 独立代码复核发现的预按修饰键别名、关闭/超时取消绕过释放配对问题，已增加复现测试并修复；Esc 在主键捕获后取消和排队完成消息的竞态也有回归覆盖。
- `git diff --check`：通过。
- `dist/MouseSide.exe`：427008 字节（417 KiB），x64。依赖仅为 Windows 系统 DLL，包括系统 `msvcrt.dll`，不需要 WSL、MinGW 或额外 C++ 运行库。

构建命令：

```sh
cmake -S . -B build-fixed -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
cmake --build build-fixed --parallel 4
ctest --test-dir build-fixed --output-on-failure
```

## 测试边界

回归测试编译真实实现，直接调用窗口和钩子回调；全局输入、键盘钩子安装、托盘操作和错误弹窗等桌面系统边界由测试替代，避免干扰使用中的应用。配置测试使用真实 Win32 INI API 和独立临时目录，并验证只读文件写入失败。测试不修改用户的 MouseSide 配置或自启注册表项。

本次没有进行真实鼠标点击和剪贴板操作、蓝牙/2.4GHz 切换、Explorer 实际重启、睡眠唤醒或长期资源占用测试。按住 Alt/Win 时目标应用是否产生菜单或焦点副作用也需要人工验证。MSVC 构建命令和静态运行库配置保留，但本次未实际使用 MSVC 编译。

## 执行决定

- 在已有项目目录的修复分支上工作，保留原始快照；没有引入新的应用框架、运行环境或在线构建流程。
- 取消录制时立即关闭录制 UI、放行新的输入，只继续吞掉此前被拦截按键的释放事件；随后清理监听，避免取消后阻断正常键盘使用。
- 更新时应先退出旧实例；旧版托盘失效时可通过任务管理器结束它。重复启动只唤醒已有实例，不会替换旧程序。

源码、回归测试和修复版 EXE 一并交付。个人提供的原始需求和内部工作计划仅在本地保留。计划中的集成目标为用户指定的 `origin/main`，推送结果以最终远程提交核验为准。
