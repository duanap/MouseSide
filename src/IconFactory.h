// IconFactory.h
//
// 运行时绘制的应用 / 托盘图标。
//
// 之所以不用 .ico 资源加载托盘图标：Windows 会按当前 DPI 缩放托盘图标，
// 直接按 SM_CXSMICON 尺寸绘制可以得到更锐利的边缘，也便于区分运行 / 暂停两态。
//
// 几何定义与 tools/make_icon.py 保持一致，修改时请同步两侧。

#pragma once

#include <windows.h>

namespace ms {
namespace IconFactory {

// 运行态（蓝色鼠标）/ 暂停态（灰色暂停条）
//
// 适用于系统托盘的图标（按 SM_CXSMICON 尺寸绘制）
HICON CreateTrayIcon(bool paused);

// 适用于窗口标题栏 / 任务栏的图标
HICON CreateAppIcon(bool paused);

}  // namespace IconFactory
}  // namespace ms
