// 回归测试：快捷键名称序列化必须可往返（保存后重启能读回）。
// 构建：cmake --build build --target vkmap_test；运行：ctest 或直接执行。
#include "../src/vkmap.h"

#include <cstdio>

static int g_fails = 0;

static void Fail(const char* what, const std::wstring& text, UINT vk) {
    ++g_fails;
    wprintf(L"FAIL %hs text=\"%s\" vk=0x%02X\n", what, text.c_str(), vk);
}

// 每个可能的 VK 都要能：名称化 → 解析回同一键
static void CheckRoundtrip(UINT vk, bool ctrl, bool shift, bool alt, bool win) {
    Hotkey hk;
    hk.ctrl = ctrl; hk.shift = shift; hk.alt = alt; hk.win = win; hk.vk = vk;
    const std::wstring text = HotkeyToText(hk);
    Hotkey back;
    if (!HotkeyFromText(text, &back))
        Fail("parse-failed", text, vk);
    else if (!(back == hk))
        Fail("mismatch", text, vk);
}

int main() {
    for (UINT vk = 1; vk < 256; ++vk) {
        CheckRoundtrip(vk, false, false, false, false);
        CheckRoundtrip(vk, true, false, true, false);
        CheckRoundtrip(vk, true, true, false, true);
    }

    Hotkey hk;
    // 常规组合
    if (!HotkeyFromText(L"Ctrl+Shift+S", &hk) || !hk.ctrl || !hk.shift ||
        hk.alt || hk.win || hk.vk != L'S')
        Fail("basic", L"Ctrl+Shift+S", 0);
    // Win+D 与 Alt+Tab（PRD 示例）
    if (!HotkeyFromText(L"Win+D", &hk) || !hk.win || hk.vk != L'D')
        Fail("wind", L"Win+D", 0);
    if (!HotkeyFromText(L"Alt+Tab", &hk) || !hk.alt || hk.vk != VK_TAB)
        Fail("alttab", L"Alt+Tab", 0);
    // 旧版本把 VK_ADD 写成 "Num+"：'+' 被当分隔符后应仍能读回（Num → 失败是
    // 预期，但完整文本 "Ctrl+Num+" 旧版会丢键）。新格式 NumAdd 必须正确。
    if (!HotkeyFromText(L"NumAdd", &hk) || hk.vk != VK_ADD)
        Fail("numadd", L"NumAdd", 0);
    if (!HotkeyFromText(L"Ctrl+NumAdd", &hk) || !hk.ctrl || hk.vk != VK_ADD)
        Fail("ctrl-numadd", L"Ctrl+NumAdd", 0);
    // 兜底格式（未收录键）
    if (!HotkeyFromText(L"VK 0xBA", &hk) || hk.vk != 0xBA)
        Fail("vkfmt", L"VK 0xBA", 0);
    if (!HotkeyFromText(L"Ctrl+VK 0xDB", &hk) || !hk.ctrl || hk.vk != VK_OEM_4)
        Fail("vkfmt-mod", L"Ctrl+VK 0xDB", 0);
    // 非法输入必须拒绝
    if (HotkeyFromText(L"Ctrl+Ctrl+A", &hk)) Fail("dup-mod", L"Ctrl+Ctrl+A", 0);
    if (HotkeyFromText(L"Bogus", &hk)) Fail("bogus", L"Bogus", 0);
    if (HotkeyFromText(L"Ctrl+", &hk)) Fail("trailing", L"Ctrl+", 0);
    if (HotkeyFromText(L"", &hk)) Fail("empty", L"", 0);

    if (g_fails == 0) {
        printf("vkmap_test: ALL PASS\n");
        return 0;
    }
    printf("vkmap_test: %d FAILURES\n", g_fails);
    return 1;
}
