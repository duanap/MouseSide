#include "Input.h"
#include "Test.h"
#include <vector>
using namespace ms;
static std::vector<KeyEvent> received;
static unsigned rawMouseEvents=0;
static unsigned menuActivations=0;
static bool acceptSystemKeys=false;
static std::wstring characters;
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
    if(message==WM_SYSCOMMAND && (w & 0xFFF0u)==SC_KEYMENU) { ++menuActivations; return 0; }
    if(message==WM_CHAR) characters += static_cast<wchar_t>(w);
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP) {
        received.push_back({static_cast<unsigned>(w), message == WM_KEYUP || message == WM_SYSKEYUP});
        if(acceptSystemKeys && (message==WM_SYSKEYDOWN || message==WM_SYSKEYUP)) return DefWindowProcW(window,message,w,l);
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
    std::array<bool,256> injectedKeys{};
    try {
        std::array<bool,256> held{};
        auto sequence = buildKeySequence({'V', Ctrl},held);
        CHECK(sequence.count == 4);
        CHECK(sequence.events[0] == KeyEvent{VK_LCONTROL,false});
        CHECK(sequence.events[1] == KeyEvent{'V',false});
        CHECK(sequence.events[2] == KeyEvent{'V',true});
        CHECK(sequence.events[3] == KeyEvent{VK_LCONTROL,true});
        held={}; held[VK_LWIN]=true;
        sequence=buildKeySequence({'F',Ctrl},held);
        CHECK(sequence.count==10 && sequence.events[0] == KeyEvent{0xFF,false});
        CHECK(sequence.events[1] == KeyEvent{0xFF,true});
        CHECK(sequence.events[2] == KeyEvent{VK_LWIN,true});
        CHECK(sequence.events[7] == KeyEvent{VK_LWIN,false});
        CHECK(sequence.events[8] == KeyEvent{0xFF,false} && sequence.events[9] == KeyEvent{0xFF,true});
        held={};
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
        CHECK(sequence.count == 4 && sequence.events[0] == KeyEvent{'C',true});
        CHECK(sequence.events[2] == KeyEvent{'C',false});
        CHECK(sequence.events[3] == KeyEvent{VK_LCONTROL,true});

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
        auto rawKey=[&](unsigned key,bool up) {
            INPUT event{}; event.type=INPUT_KEYBOARD; event.ki.wVk=static_cast<WORD>(key); event.ki.dwFlags=up ? KEYEVENTF_KEYUP : 0;
            CHECK(SendInput(1,&event,sizeof(INPUT))==1);
            injectedKeys[key]=!up;
        };
        rawKey('C',false); pump(); received.clear(); characters.clear();
        CHECK(sendShortcut({'C',Ctrl},error)); pump();
        CHECK(characters.find(L'c')==std::wstring::npos && characters.find(L'C')==std::wstring::npos);
        CHECK(GetAsyncKeyState('C') & 0x8000);
        rawKey('C',true); pump();
        acceptSystemKeys=true; menuActivations=0;
        rawKey(VK_LMENU,false); pump();
        CHECK(sendShortcut({'F',Ctrl},error)); pump();
        CHECK(menuActivations==0);
        rawKey(VK_LMENU,true); pump();
        CHECK(menuActivations==0);
        acceptSystemKeys=false; received.clear();
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
    } catch(const std::exception& e) {
        for(unsigned key=0;key<256;++key) if(injectedKeys[key]) {
            INPUT event{}; event.type=INPUT_KEYBOARD; event.ki.wVk=static_cast<WORD>(key); event.ki.dwFlags=KEYEVENTF_KEYUP;
            SendInput(1,&event,sizeof(INPUT));
        }
        if(observer) UnhookWindowsHookEx(observer);
        if(window) DestroyWindow(window);
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
}
