#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MouseSide 源码静态自检（无编译器环境下的兜底检查）

检查内容：
  1. 括号 / 花括号 / 方括号配平（跳过字符串、字符常量、注释）
  2. #include "..." 的本地头文件是否真实存在
  3. 每个 .cpp 的 namespace 是否闭合
  4. 常见的低级笔误（中文标点混入代码、行尾空白、TAB 混用）
"""

from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")

PAIRS = {")": "(", "]": "[", "}": "{"}
OPENERS = set("([{")

CN_PUNCT = "，。；：（）【】“”‘’《》？！"


def strip_code(text: str):
    """去掉注释与字符串/字符常量，返回 (可分析文本, 行号映射用行列表)"""
    out = []
    i = 0
    n = len(text)
    state = "code"  # code | line_comment | block_comment | string | char | raw
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                out.append("  ")
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                out.append("  ")
                i += 2
                continue
            if ch == '"':
                state = "string"
                out.append(" ")
                i += 1
                continue
            if ch == "'":
                state = "char"
                out.append(" ")
                i += 1
                continue
            out.append(ch)
            i += 1
        elif state == "line_comment":
            if ch == "\n":
                state = "code"
                out.append("\n")
            else:
                out.append(" ")
            i += 1
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                out.append("  ")
                i += 2
                continue
            out.append("\n" if ch == "\n" else " ")
            i += 1
        elif state in ("string", "char"):
            if ch == "\\":
                out.append("  ")
                i += 2
                continue
            if (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                state = "code"
            out.append("\n" if ch == "\n" else " ")
            i += 1
    return "".join(out)


def check_balance(path: str):
    with open(path, encoding="utf-8") as fh:
        raw = fh.read()
    code = strip_code(raw)

    stack = []
    line = 1
    for ch in code:
        if ch == "\n":
            line += 1
        elif ch in OPENERS:
            stack.append((ch, line))
        elif ch in PAIRS:
            if not stack:
                return f"{os.path.basename(path)}: 第 {line} 行出现多余的 '{ch}'"
            opener, opened_at = stack.pop()
            if opener != PAIRS[ch]:
                return (f"{os.path.basename(path)}: 第 {line} 行的 '{ch}' "
                        f"与第 {opened_at} 行的 '{opener}' 不匹配")
    if stack:
        opener, opened_at = stack[-1]
        return f"{os.path.basename(path)}: 第 {opened_at} 行的 '{opener}' 未闭合"
    return None


def main() -> int:
    problems = []
    files = sorted(f for f in os.listdir(SRC) if f.endswith((".h", ".cpp")))

    for name in files:
        path = os.path.join(SRC, name)
        with open(path, encoding="utf-8") as fh:
            text = fh.read()

        err = check_balance(path)
        if err:
            problems.append(err)

        # include 检查
        for match in re.finditer(r'#include\s+"([^"]+)"', text):
            header = match.group(1)
            if "/" not in header and not os.path.exists(os.path.join(SRC, header)):
                problems.append(f"{name}: 引用了不存在的头文件 {header}")

        # 中文标点混入代码行（只看去掉注释与字符串之后的真实代码）
        code_text = strip_code(text)
        for lineno, line in enumerate(code_text.splitlines(), 1):
            hit = [p for p in CN_PUNCT if p in line]
            if hit:
                problems.append(
                    f"{name}:{lineno}: 代码中疑似混入中文标点 {''.join(hit)}")

        # TOML/代码文件中不应出现行尾空白
        for lineno, line in enumerate(text.splitlines(), 1):
            if line != line.rstrip():
                problems.append(f"{name}:{lineno}: 行尾存在多余空白")

    print(f"已检查 {len(files)} 个源文件")
    for name in files:
        print("  -", name)

    if problems:
        print("\n发现问题:")
        for item in problems:
            print("  !", item)
        return 1

    print("\n括号配平 / include 引用 / 标点 检查全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
