#include "Model.h"
#include "Config.h"
#include "Test.h"
#include <filesystem>
#include <fstream>

using namespace ms;
int main() {
  try {
    const Config defaults;
    CHECK(defaults.buttons[0].shortcut == Shortcut{'V', Ctrl});
    CHECK(defaults.buttons[1].shortcut == Shortcut{'C', Ctrl});
    CHECK(defaults.enabled && !defaults.autostart);
    CHECK(!validShortcut({VK_CONTROL, Ctrl}));
    CHECK(!validShortcut({0, 0}));
    CHECK(!validShortcut({VK_DELETE, Ctrl | Alt}));
    CHECK(!validShortcut({'A', 0x80}));
    CHECK(validShortcut({VK_TAB, Alt}));
    CHECK(validShortcut({'D', Win}));
    RecordingState record;
    CHECK(record.process(VK_LWIN,true) == RecordingDecision::Swallow);
    CHECK(record.process('D',true) == RecordingDecision::Swallow);
    CHECK(!record.complete());
    CHECK(record.process('D',false) == RecordingDecision::Swallow);
    CHECK(record.process(VK_LWIN,false) == RecordingDecision::Swallow);
    CHECK(record.complete() && record.shortcut() == Shortcut{'D',Win});
    record = {};
    record.process(VK_LMENU,true); record.process(VK_TAB,true);
    record.process(VK_TAB,false); record.process(VK_LMENU,false);
    CHECK(record.complete() && record.shortcut() == Shortcut{VK_TAB,Alt});
    record = {};
    record.process(VK_LCONTROL,true); record.cancel();
    CHECK(!record.complete());
    CHECK(record.process('Z',true) == RecordingDecision::Pass);
    record.process(VK_LCONTROL,false);
    CHECK(record.complete() && !record.shortcut());
    record = {};
    record.process(VK_ESCAPE,true); record.process(VK_ESCAPE,false);
    CHECK(record.complete() && !record.shortcut());
    record = {};
    record.process(VK_LCONTROL,true); record.process(VK_LMENU,true); record.process(VK_DELETE,true);
    record.process(VK_DELETE,false); record.process(VK_LMENU,false); record.process(VK_LCONTROL,false);
    CHECK(record.complete() && record.invalid() && !record.shortcut());
    CHECK(decodeAction(L"shortcut:2:86") == defaults.buttons[0]);
    CHECK(!decodeAction(L"shortcut:99:86"));
    CHECK(!decodeAction(L"shortcut:2:17"));
    CHECK(!decodeAction(L"shortcut:2:86junk"));
    CHECK(!decodeAction(L"shortcut:-1:86"));
    CHECK(!decodeAction(L"shortcut:0:256"));
    CHECK(decodeAction(L"original")->kind == ActionKind::Original);
    CHECK(decodeAction(L"disabled")->kind == ActionKind::Disabled);
    ButtonState state;
    CHECK(state.onDown(true, defaults.buttons[0]) == ButtonDecision::Dispatch);
    CHECK(state.onDown(true, defaults.buttons[0]) == ButtonDecision::Swallow);
    CHECK(state.onUp() == ButtonDecision::Swallow);
    CHECK(state.onUp() == ButtonDecision::Pass);
    CHECK(state.onDown(false, defaults.buttons[0]) == ButtonDecision::Pass);
    CHECK(state.onDown(true, defaults.buttons[0]) == ButtonDecision::Pass);
    CHECK(state.onUp() == ButtonDecision::Pass);
    CHECK(state.onDown(true, Action{ActionKind::Disabled, {}}) == ButtonDecision::Swallow);
    CHECK(state.onUp() == ButtonDecision::Swallow);
    CHECK(state.onDown(true, defaults.buttons[0]) == ButtonDecision::Dispatch);
    state.dispatchFailed();
    CHECK(state.onUp() == ButtonDecision::Pass);

    const auto dir = std::filesystem::temp_directory_path() / (L"MouseSide-codex-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(dir);
    ConfigStore store(dir / L"config.ini");
    bool repaired = false;
    CHECK(store.load(repaired) == defaults);
    CHECK(repaired);
    std::wstring error;
    CHECK(store.save(defaults, error));
    CHECK(store.load(repaired) == defaults && !repaired);
    Config custom = defaults;
    custom.enabled = false;
    custom.autostart = true;
    custom.buttons[0] = {ActionKind::Shortcut, {'S', Ctrl | Shift}};
    custom.buttons[1] = {ActionKind::Original, {}};
    CHECK(store.save(custom, error));
    CHECK(store.load(repaired) == custom && !repaired);
    {
      std::ofstream damaged(dir / L"config.ini", std::ios::binary | std::ios::trunc);
      damaged << "[MouseSide]\nButton4=shortcut:999:86\nButton5=disabled\nEnabled=oops\nAutoStart=0\n";
    }
    const Config recovered = store.load(repaired);
    CHECK(repaired && recovered.buttons[0] == defaults.buttons[0]);
    CHECK(recovered.buttons[1].kind == ActionKind::Disabled);
    CHECK(recovered.enabled && !recovered.autostart);
    ConfigStore blocked(dir); // existing directory cannot be replaced by a file
    CHECK(!blocked.save(custom, error) && !error.empty());
    std::filesystem::remove_all(dir);
    std::cout << "PASS: defaults, shortcut validation, strict parsing, paired clicks, atomic config persistence/recovery\n";
    return 0;
  } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
