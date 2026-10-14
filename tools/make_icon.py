#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MouseSide 图标生成器（纯标准库实现，无需 Pillow）

用法：
    python tools/make_icon.py

产出：
    assets/MouseSide.ico    多尺寸应用图标（16 / 24 / 32 / 48 / 64 / 128 / 256）

图标几何与 src/IconFactory.cpp 中运行时绘制的托盘图标保持一致，
修改任意一侧时应同步另一侧。
"""

from __future__ import annotations

import os
import struct
import zlib

# --------------------------------------------------------------------------
# 配色
# --------------------------------------------------------------------------

BG_TOP = (0x4A, 0x7D, 0xFF)
BG_BOTTOM = (0x24, 0x50, 0xE8)
GLYPH = (0xFF, 0xFF, 0xFF)

PAUSED_BG_TOP = (0xB4, 0xBA, 0xC4)
PAUSED_BG_BOTTOM = (0x8B, 0x92, 0x9D)

SUPERSAMPLE = 4  # 每像素 4x4 子采样，用于抗锯齿


# --------------------------------------------------------------------------
# 几何判定（全部使用 0..1 归一化坐标）
# --------------------------------------------------------------------------


def in_rounded_rect(x: float, y: float,
                    x0: float, y0: float, x1: float, y1: float,
                    r: float) -> bool:
    """点是否落在圆角矩形内。"""
    if x < x0 or x > x1 or y < y0 or y > y1:
        return False
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    dx = x - cx
    dy = y - cy
    return dx * dx + dy * dy <= r * r


def _lerp(a: int, b: int, t: float) -> int:
    return int(round(a + (b - a) * t))


def _gradient(top, bottom, t: float):
    return (_lerp(top[0], bottom[0], t),
            _lerp(top[1], bottom[1], t),
            _lerp(top[2], bottom[2], t))


def sample_running(x: float, y: float):
    """运行状态：蓝色圆角方块 + 白色鼠标剪影。返回 (r, g, b, a)。"""
    if not in_rounded_rect(x, y, 0.03, 0.03, 0.97, 0.97, 0.24):
        return (0, 0, 0, 0)

    bg = _gradient(BG_TOP, BG_BOTTOM, y)

    body = in_rounded_rect(x, y, 0.310, 0.155, 0.690, 0.845, 0.190)
    if body:
        split_h = 0.398 < y < 0.442                        # 左右键分界线
        split_v = (0.482 < x < 0.518) and (y < 0.420)      # 双键中缝
        wheel = in_rounded_rect(x, y, 0.452, 0.240, 0.548, 0.352, 0.048)
        if not (split_h or split_v or wheel):
            return (GLYPH[0], GLYPH[1], GLYPH[2], 255)

    return (bg[0], bg[1], bg[2], 255)


def sample_paused(x: float, y: float):
    """暂停状态：灰色圆角方块 + 白色暂停条。返回 (r, g, b, a)。"""
    if not in_rounded_rect(x, y, 0.03, 0.03, 0.97, 0.97, 0.24):
        return (0, 0, 0, 0)

    bg = _gradient(PAUSED_BG_TOP, PAUSED_BG_BOTTOM, y)

    bar_left = in_rounded_rect(x, y, 0.340, 0.270, 0.446, 0.730, 0.052)
    bar_right = in_rounded_rect(x, y, 0.554, 0.270, 0.660, 0.730, 0.052)
    if bar_left or bar_right:
        return (GLYPH[0], GLYPH[1], GLYPH[2], 255)

    return (bg[0], bg[1], bg[2], 255)


# --------------------------------------------------------------------------
# 光栅化
# --------------------------------------------------------------------------


def render(size: int, sampler) -> bytes:
    """渲染为 size x size 的 RGBA 字节流。"""
    step = 1.0 / (size * SUPERSAMPLE)
    half = step * 0.5
    total = SUPERSAMPLE * SUPERSAMPLE
    out = bytearray(size * size * 4)

    for py in range(size):
        row_base = py * size * 4
        for px in range(size):
            acc_r = acc_g = acc_b = 0
            covered = 0
            for sy in range(SUPERSAMPLE):
                y = (py * SUPERSAMPLE + sy) * step + half
                for sx in range(SUPERSAMPLE):
                    x = (px * SUPERSAMPLE + sx) * step + half
                    r, g, b, a = sampler(x, y)
                    if a:
                        acc_r += r
                        acc_g += g
                        acc_b += b
                        covered += 1
            idx = row_base + px * 4
            if covered == 0:
                out[idx:idx + 4] = b"\x00\x00\x00\x00"
            else:
                out[idx + 0] = acc_r // covered
                out[idx + 1] = acc_g // covered
                out[idx + 2] = acc_b // covered
                out[idx + 3] = (covered * 255) // total
    return bytes(out)


# --------------------------------------------------------------------------
# PNG / ICO 编码
# --------------------------------------------------------------------------


def _png_chunk(tag: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def png_encode(size: int, rgba: bytes) -> bytes:
    stride = size * 4
    raw = bytearray()
    for y in range(size):
        raw.append(0)  # filter type: None
        raw.extend(rgba[y * stride:(y + 1) * stride])

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n"
            + _png_chunk(b"IHDR", header)
            + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _png_chunk(b"IEND", b""))


def build_ico(entries) -> bytes:
    """entries: [(size, png_bytes), ...]"""
    count = len(entries)
    header = struct.pack("<HHH", 0, 1, count)

    offset = 6 + 16 * count
    dir_bytes = bytearray()
    body = bytearray()
    for size, png in entries:
        dim = 0 if size >= 256 else size  # 0 表示 256
        dir_bytes += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(png), offset)
        offset += len(png)
        body += png
    return header + bytes(dir_bytes) + bytes(body)


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

ICON_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    assets = os.path.normpath(os.path.join(here, "..", "assets"))
    os.makedirs(assets, exist_ok=True)

    entries = []
    for size in ICON_SIZES:
        rgba = render(size, sample_running)
        entries.append((size, png_encode(size, rgba)))
        print(f"  running  {size:>3}x{size}")

    ico_path = os.path.join(assets, "MouseSide.ico")
    with open(ico_path, "wb") as fh:
        fh.write(build_ico(entries))
    print(f"\n已生成: {ico_path}  ({os.path.getsize(ico_path)} bytes)")

    # 同时导出一张 256px PNG，便于预览检查
    preview = os.path.join(here, "preview_running_256.png")
    with open(preview, "wb") as fh:
        fh.write(png_encode(256, render(256, sample_running)))
    preview_paused = os.path.join(here, "preview_paused_256.png")
    with open(preview_paused, "wb") as fh:
        fh.write(png_encode(256, render(256, sample_paused)))
    print(f"预览图: {preview}")
    print(f"预览图: {preview_paused}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
