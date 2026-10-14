#include "Input.h"
#include <algorithm>
namespace ms {
constexpr std::array<unsigned,8> modifierKeys{VK_LCONTROL,VK_RCONTROL,VK_LMENU,VK_RMENU,VK_LSHIFT,VK_RSHIFT,VK_LWIN,VK_RWIN};
KeySequence buildKeySequence(Shortcut shortcut, const std::array<bool,256>& held) {
    KeySequence sequence;
    if (!validShortcut(shortcut)) return sequence;
    auto add = [&](unsigned key, bool up) { sequence.events[sequence.count++] = {key,up}; };
    std::array<bool,8> released{};
    std::array<unsigned,4> added{};
    unsigned addedCount = 0;
    if (held[shortcut.key]) add(shortcut.key,true);
    for (unsigned i=0; i<modifierKeys.size(); ++i) {
        const unsigned key = modifierKeys[i];
        if (held[key] && !(shortcut.modifiers & modifierFor(key))) { add(key,true); released[i]=true; }
    }
    for (unsigned family : {Ctrl,Alt,Shift,Win}) {
        if (!(shortcut.modifiers & family)) continue;
        bool exists = false;
        for (unsigned key : modifierKeys) if (modifierFor(key)==family && held[key]) exists=true;
        if (!exists) {
            const unsigned key = family==Ctrl ? VK_LCONTROL : family==Alt ? VK_LMENU : family==Shift ? VK_LSHIFT : VK_LWIN;
            add(key,false); added[addedCount++]=key;
        }
    }
    add(shortcut.key,false); add(shortcut.key,true);
    while (addedCount) add(added[--addedCount],true);
    for (unsigned i=0; i<modifierKeys.size(); ++i) if (released[i]) add(modifierKeys[i],false);
    if (held[shortcut.key]) add(shortcut.key,false);
    return sequence;
}
static INPUT keyboardInput(KeyEvent key) {
    INPUT input{}; input.type=INPUT_KEYBOARD;
    input.ki.wVk=static_cast<WORD>(key.key);
    input.ki.dwExtraInfo=injectionTag;
    if (key.up) input.ki.dwFlags|=KEYEVENTF_KEYUP;
    const UINT scan=MapVirtualKeyW(key.key,MAPVK_VK_TO_VSC_EX);
    if ((scan & 0xFF00u)==0xE000u || key.key==VK_RCONTROL || key.key==VK_RMENU || key.key==VK_LWIN || key.key==VK_RWIN)
        input.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;
    return input;
}
bool sendShortcut(Shortcut shortcut, DWORD& error) {
    if (!validShortcut(shortcut)) { error=ERROR_INVALID_DATA; return false; }
    std::array<bool,256> held{};
    for (unsigned key : modifierKeys) held[key]=(GetAsyncKeyState(static_cast<int>(key)) & 0x8000)!=0;
    held[shortcut.key]=(GetAsyncKeyState(static_cast<int>(shortcut.key)) & 0x8000)!=0;
    const KeySequence sequence=buildKeySequence(shortcut,held);
    std::array<INPUT,24> inputs{};
    for (unsigned i=0; i<sequence.count; ++i) inputs[i]=keyboardInput(sequence.events[i]);
    SetLastError(0);
    const UINT sent=SendInput(sequence.count,inputs.data(),sizeof(INPUT));
    if (sent==sequence.count) { error=0; return true; }
    error=GetLastError();
    if (!error) error=ERROR_ACCESS_DENIED;
    // Only restore keys whose state this partially inserted batch changed.
    auto state=held;
    for (unsigned i=0; i<sent; ++i) state[sequence.events[i].key]=!sequence.events[i].up;
    unsigned count=0;
    for (unsigned key=0; key<256; ++key) if (state[key]!=held[key]) inputs[count++]=keyboardInput({key,!held[key]});
    if (count) SendInput(count,inputs.data(),sizeof(INPUT));
    return false;
}
thread_local InputEngine* InputEngine::current_=nullptr;
constexpr UINT dispatchMessage=WM_APP+21;
bool InputEngine::start(HWND owner,const Config& config,DWORD& error) {
    stop();
    owner_=owner; states_={}; installError_=0; update(config);
    ready_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (!ready_) { error=GetLastError(); return false; }
    thread_=CreateThread(nullptr,0,threadEntry,this,0,&threadId_);
    if (!thread_) { error=GetLastError(); CloseHandle(ready_); ready_=nullptr; return false; }
    WaitForSingleObject(ready_,INFINITE);
    CloseHandle(ready_); ready_=nullptr;
    error=installError_;
    if (error) { stop(); return false; }
    return true;
}
void InputEngine::update(const Config& config) {
    for (unsigned i=0; i<2; ++i) actions_[i].store(packAction(config.buttons[i]),std::memory_order_release);
    enabled_.store(config.enabled,std::memory_order_release);
}
void InputEngine::stop() {
    if (thread_) {
        PostThreadMessageW(threadId_,WM_QUIT,0,0);
        WaitForSingleObject(thread_,INFINITE);
        CloseHandle(thread_); thread_=nullptr; threadId_=0;
    }
}
DWORD WINAPI InputEngine::threadEntry(void* context) {
    auto& engine=*static_cast<InputEngine*>(context);
    current_=&engine;
    MSG message{};
    PeekMessageW(&message,nullptr,WM_USER,WM_USER,PM_NOREMOVE); // create message queue before publishing ready
    engine.hook_=SetWindowsHookExW(WH_MOUSE_LL,hookProc,GetModuleHandleW(nullptr),0);
    if (!engine.hook_) engine.installError_=GetLastError();
    SetEvent(engine.ready_);
    if (engine.hook_) {
        while (GetMessageW(&message,nullptr,0,0)>0) {
            if (message.message==dispatchMessage) {
                DWORD error=0;
                if (GetForegroundWindow()!=reinterpret_cast<HWND>(message.lParam)) continue;
                if (!sendShortcut(unpackAction(static_cast<unsigned>(message.wParam)).shortcut,error)) {
                    engine.enabled_.store(false);
                    PostMessageW(engine.owner_,inputErrorMessage,error,0);
                }
            }
        }
        UnhookWindowsHookEx(engine.hook_); engine.hook_=nullptr;
    }
    current_=nullptr;
    return 0;
}
LRESULT CALLBACK InputEngine::hookProc(int code,WPARAM w,LPARAM l) {
    if (code<0 || (w!=WM_XBUTTONDOWN && w!=WM_XBUTTONUP)) return CallNextHookEx(nullptr,code,w,l);
    const auto* event=reinterpret_cast<const MSLLHOOKSTRUCT*>(l);
    if (event->dwExtraInfo==injectionTag) return CallNextHookEx(nullptr,code,w,l);
    const unsigned button=HIWORD(event->mouseData);
    if (button!=XBUTTON1 && button!=XBUTTON2) return CallNextHookEx(nullptr,code,w,l);
    auto& engine=*current_;
    auto& state=engine.states_[button==XBUTTON1 ? 0 : 1];
    ButtonDecision decision;
    if (w==WM_XBUTTONDOWN) {
        const unsigned packed=engine.actions_[button==XBUTTON1 ? 0 : 1].load(std::memory_order_acquire);
        decision=state.onDown(engine.enabled_.load(std::memory_order_acquire),unpackAction(packed));
        if (decision==ButtonDecision::Dispatch && !PostThreadMessageW(engine.threadId_,dispatchMessage,packed,reinterpret_cast<LPARAM>(GetForegroundWindow()))) {
            state.dispatchFailed(); decision=ButtonDecision::Pass;
        }
    } else decision=state.onUp();
    if (decision!=ButtonDecision::Pass) return 1;
    return CallNextHookEx(nullptr,code,w,l);
}
ShortcutRecorder* ShortcutRecorder::current_=nullptr;
bool ShortcutRecorder::begin(HWND owner,DWORD& error) {
    if (hook_) { error=ERROR_BUSY; return false; }
    // Starting while a keyboard key is already held could swallow an unmatched release.
    for (int key=VK_BACK; key<256; ++key) if (GetAsyncKeyState(key)&0x8000) { error=ERROR_BUSY; return false; }
    owner_=owner; state_={}; current_=this;
    hook_=SetWindowsHookExW(WH_KEYBOARD_LL,hookProc,GetModuleHandleW(nullptr),0);
    if (!hook_) { error=GetLastError(); current_=nullptr; return false; }
    error=0; return true;
}
void ShortcutRecorder::cancel() {
    if (!hook_) return;
    state_.cancel();
    if (state_.complete())
        PostMessageW(owner_,recordedMessage,0,0);
}
void ShortcutRecorder::finish() {
    if (hook_) { UnhookWindowsHookEx(hook_); hook_=nullptr; }
    if (current_==this) current_=nullptr;
}
LRESULT CALLBACK ShortcutRecorder::hookProc(int code,WPARAM w,LPARAM l) {
    if (code<0) return CallNextHookEx(nullptr,code,w,l);
    const auto* event=reinterpret_cast<const KBDLLHOOKSTRUCT*>(l);
    if (event->flags & LLKHF_INJECTED) return CallNextHookEx(nullptr,code,w,l);
    auto& recorder=*current_;
    const unsigned key=event->vkCode;
    if (key>=256) return CallNextHookEx(nullptr,code,w,l);
    const bool down=w==WM_KEYDOWN || w==WM_SYSKEYDOWN;
    if (GetForegroundWindow()!=recorder.owner_) recorder.state_.cancel();
    const auto decision=recorder.state_.process(key,down);
    if (recorder.state_.complete()) {
        auto shortcut=recorder.state_.shortcut();
        const unsigned result=shortcut ? packAction({ActionKind::Shortcut,*shortcut}) : 0;
        PostMessageW(recorder.owner_,recordedMessage,result,recorder.state_.invalid() ? 1 : 0);
    }
    return decision==RecordingDecision::Swallow ? 1 : CallNextHookEx(nullptr,code,w,l);
}
}
