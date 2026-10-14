#include "Input.h"
#include "Test.h"
#include <vector>
using namespace ms;
static std::vector<KeyEvent> received;
static unsigned rawMouseEvents=0;
constexpr ULONG_PTR testMouseTag=0x544D5343;
static LRESULT CALLBACK rawObserver(int code,WPARAM w,LPARAM l) {
    if(code>=0 && (w==WM_XBUTTONDOWN || w==WM_XBUTTONUP) && reinterpret_cast<MSLLHOOKSTRUCT*>(l)->dwExtraInfo==testMouseTag) {
        ++rawMouseEvents;
        return 1; // isolate synthetic test input from other installed mappers
    }
    return CallNextHookEx(nullptr,code,w,l);
}
static LRESULT CALLBACK receiver(HWND window, UINT message, WPARAM w, LPARAM l) {
    if(message==WM_XBUTTONDOWN || message==WM_XBUTTONUP) { ++rawMouseEvents; return TRUE; }
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP) {
        received.push_back({static_cast<unsigned>(w), message == WM_KEYUP || message == WM_SYSKEYUP});
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
static void pump() {
    const auto until = GetTickCount64() + 150;
    MSG message;
    do { while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); } Sleep(1); } while(GetTickCount64() < until);
}
int main() {
    HWND window = nullptr;
    HHOOK observer=nullptr;
    try {
        std::array<bool,256> held{};
        auto sequence = buildKeySequence({'V', Ctrl},held);
        CHECK(sequence.count == 4);
        CHECK(sequence.events[0] == KeyEvent{VK_LCONTROL,false});
        CHECK(sequence.events[1] == KeyEvent{'V',false});
        CHECK(sequence.events[2] == KeyEvent{'V',true});
        CHECK(sequence.events[3] == KeyEvent{VK_LCONTROL,true});
        held[VK_RSHIFT] = true;
        held[VK_RCONTROL] = true;
        sequence = buildKeySequence({'V',Ctrl},held);
        CHECK(sequence.count == 4);
        CHECK(sequence.events[0] == KeyEvent{VK_RSHIFT,true});
        CHECK(sequence.events[3] == KeyEvent{VK_RSHIFT,false});
        CHECK(buildKeySequence({VK_CONTROL,Ctrl},held).count == 0);
        held = {};
        held['C'] = true;
        sequence = buildKeySequence({'C',Ctrl},held);
        CHECK(sequence.count == 6 && sequence.events[0] == KeyEvent{'C',true});
        CHECK(sequence.events[5] == KeyEvent{'C',false});

        WNDCLASSW wc{}; wc.lpfnWndProc=receiver; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"MouseSide.Codex.InputTest";
        RegisterClassW(&wc);
        window=CreateWindowW(wc.lpszClassName,L"MouseSide isolated input test",WS_OVERLAPPEDWINDOW,100,100,300,180,nullptr,nullptr,wc.hInstance,nullptr);
        CHECK(window);
        ShowWindow(window,SW_SHOW);
        // Windows foreground-lock policy blocks automation launched from a background terminal.
        // A balanced Alt gesture enables activation; then all test shortcuts target this window.
        INPUT activation[2]{};
        for(auto& event : activation) { event.type=INPUT_KEYBOARD; event.ki.wVk=VK_MENU; }
        activation[1].ki.dwFlags=KEYEVENTF_KEYUP;
        SendInput(2,activation,sizeof(INPUT));
        SetForegroundWindow(window); SetFocus(window); pump(); received.clear();
        CHECK(GetForegroundWindow()==window);
        DWORD error=0;
        CHECK(sendShortcut({'F',Ctrl},error)); pump();
        if(received.size()!=4) for(const auto& key : received) std::cout << "received: " << key.key << " up=" << key.up << '\n';
        CHECK(received.size() == 4);
        CHECK(received[0] == KeyEvent{VK_CONTROL,false});
        CHECK(received[1] == KeyEvent{'F',false});
        CHECK(received[2] == KeyEvent{'F',true});
        CHECK(received[3] == KeyEvent{VK_CONTROL,true});
        CHECK(!(GetAsyncKeyState(VK_CONTROL)&0x8000));
        InputEngine engine;
        Config config;
        config.buttons[0] = {ActionKind::Shortcut,{'F',Ctrl}};
        observer=SetWindowsHookExW(WH_MOUSE_LL,rawObserver,GetModuleHandleW(nullptr),0);
        CHECK(observer);
        CHECK(engine.start(window,config,error));
        received.clear();
        INPUT mouse[2]{};
        for (auto& event : mouse) { event.type=INPUT_MOUSE; event.mi.mouseData=XBUTTON1; event.mi.dwExtraInfo=testMouseTag; }
        mouse[0].mi.dwFlags=MOUSEEVENTF_XDOWN; mouse[1].mi.dwFlags=MOUSEEVENTF_XUP;
        CHECK(SendInput(2,mouse,sizeof(INPUT))==2); pump();
        CHECK(received.size()==4 && received[1].key=='F');
        CHECK(rawMouseEvents==0);
        config.enabled=false; engine.update(config); received.clear(); rawMouseEvents=0;
        CHECK(SendInput(2,mouse,sizeof(INPUT))==2); pump();
        CHECK(received.empty());
        CHECK(rawMouseEvents==2);
        engine.stop();
        UnhookWindowsHookEx(observer); observer=nullptr;
        DestroyWindow(window); window=nullptr;
        std::cout << "PASS: balanced input, held modifiers, real SendInput receiver, real side-button hook, pause\n";
        return 0;
    } catch(const std::exception& e) { if(observer) UnhookWindowsHookEx(observer); if(window) DestroyWindow(window); std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
