#pragma once
#include "Model.h"
#include <atomic>
namespace ms {
constexpr ULONG_PTR injectionTag = 0x4D534344; // MSCD
constexpr UINT inputErrorMessage = WM_APP + 12;
constexpr UINT recordedMessage = WM_APP + 13;
struct KeyEvent { unsigned key = 0; bool up = false; bool operator==(const KeyEvent&) const = default; };
struct KeySequence { std::array<KeyEvent, 24> events{}; unsigned count = 0; };
KeySequence buildKeySequence(Shortcut shortcut, const std::array<bool, 256>& held);
bool sendShortcut(Shortcut shortcut, DWORD& error);
class InputEngine {
public:
    ~InputEngine() { stop(); }
    bool start(HWND owner, const Config& config, DWORD& error);
    void update(const Config& config);
    void stop();
private:
    static DWORD WINAPI threadEntry(void* context);
    static LRESULT CALLBACK hookProc(int code, WPARAM wparam, LPARAM lparam);
    static thread_local InputEngine* current_;
    HANDLE thread_ = nullptr;
    HANDLE ready_ = nullptr;
    DWORD threadId_ = 0;
    DWORD installError_ = 0;
    HWND owner_ = nullptr;
    HHOOK hook_ = nullptr;
    std::atomic<bool> enabled_{false};
    std::array<std::atomic<unsigned>, 2> actions_{};
    std::array<ButtonState, 2> states_{};
};
class ShortcutRecorder {
public:
    ~ShortcutRecorder() { finish(); }
    bool begin(HWND owner, DWORD& error);
    void cancel();
    void finish();
    bool active() const { return hook_ != nullptr; }
private:
    static LRESULT CALLBACK hookProc(int code, WPARAM wparam, LPARAM lparam);
    static ShortcutRecorder* current_;
    HHOOK hook_ = nullptr;
    HWND owner_ = nullptr;
    RecordingState state_;
};
}
