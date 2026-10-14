#pragma once
#include <windows.h>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace ms {
enum Modifier : unsigned { Shift = 1, Ctrl = 2, Alt = 4, Win = 8 };
struct Shortcut {
    unsigned key = 0;
    unsigned modifiers = 0;
    bool operator==(const Shortcut&) const = default;
};
enum class ActionKind : unsigned { Shortcut, Original, Disabled };
struct Action {
    ActionKind kind = ActionKind::Shortcut;
    Shortcut shortcut;
    bool operator==(const Action&) const = default;
};
struct Config {
    std::array<Action, 2> buttons{{{ActionKind::Shortcut, {'V', Ctrl}}, {ActionKind::Shortcut, {'C', Ctrl}}}};
    bool enabled = true;
    bool autostart = false;
    bool operator==(const Config&) const = default;
};
bool isModifier(unsigned key);
unsigned modifierFor(unsigned key);
bool validShortcut(Shortcut shortcut);
std::wstring shortcutName(Shortcut shortcut);
std::wstring actionName(Action action);
std::wstring encodeAction(Action action);
std::optional<Action> decodeAction(std::wstring_view value);
unsigned packAction(Action action);
Action unpackAction(unsigned packed);
enum class ButtonDecision { Pass, Swallow, Dispatch };
class ButtonState {
public:
    ButtonDecision onDown(bool enabled, Action action);
    ButtonDecision onUp();
    void dispatchFailed() { swallowed_ = false; }
private:
    bool down_ = false;
    bool swallowed_ = false;
};
enum class RecordingDecision { Pass, Swallow };
class RecordingState {
public:
    RecordingDecision process(unsigned key,bool down);
    void cancel() { finishing_=true; shortcut_.reset(); }
    bool complete() const;
    bool invalid() const { return invalid_; }
    std::optional<Shortcut> shortcut() const { return shortcut_; }
private:
    std::array<bool,256> held_{};
    bool finishing_=false, invalid_=false;
    std::optional<Shortcut> shortcut_;
};
}
