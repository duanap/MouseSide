#include "App.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
namespace ms {
constexpr UINT trayMessage=WM_APP+10;
constexpr UINT showMessage=WM_APP+11;
constexpr unsigned openCommand=9001, toggleCommand=9002, exitCommand=9003;
constexpr COLORREF ink=RGB(29,36,48), muted=RGB(112,121,135), blue=RGB(39,105,221);
constexpr int clientWidth=548, clientHeight=620;
struct Preset { const wchar_t* name; Action action; };
static const std::array<Preset,10> presets{{
    {L"复制",{ActionKind::Shortcut,{'C',Ctrl}}}, {L"粘贴",{ActionKind::Shortcut,{'V',Ctrl}}},
    {L"剪切",{ActionKind::Shortcut,{'X',Ctrl}}}, {L"撤销",{ActionKind::Shortcut,{'Z',Ctrl}}},
    {L"重做",{ActionKind::Shortcut,{'Y',Ctrl}}}, {L"全选",{ActionKind::Shortcut,{'A',Ctrl}}},
    {L"浏览器后退",{ActionKind::Shortcut,{VK_BROWSER_BACK,0}}}, {L"浏览器前进",{ActionKind::Shortcut,{VK_BROWSER_FORWARD,0}}},
    {L"保留原始功能",{ActionKind::Original,{}}}, {L"禁用此侧键",{ActionKind::Disabled,{}}}
}};
App::App(HINSTANCE instance) : instance_(instance),store_(ConfigStore::defaultPath()) {}
App::~App() {
    recorder_.finish(); input_.stop();
    if (trayAdded_) { NOTIFYICONDATAW icon{}; icon.cbSize=sizeof(icon); icon.hWnd=window_; icon.uID=1; Shell_NotifyIconW(NIM_DELETE,&icon); }
    if (window_ && IsWindow(window_)) DestroyWindow(window_);
    for (HFONT font : {titleFont_,bodyFont_,smallFont_,boldFont_,keyFont_}) if(font) DeleteObject(font);
    if(background_) DeleteObject(background_);
    if(white_) DeleteObject(white_);
    if(activeIcon_) DestroyIcon(activeIcon_);
    if(pausedIcon_) DestroyIcon(pausedIcon_);
}
int App::run(bool background) {
    backgroundStart_=background;
    if (!initialize()) return 1;
    MSG message{};
    int result;
    while ((result=static_cast<int>(GetMessageW(&message,nullptr,0,0)))>0) {
        if (!IsDialogMessageW(window_,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return result<0 ? 1 : 0;
}
HFONT App::makeFont(int size,int weight) {
    return CreateFontW(-scale(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
}
bool App::initialize() {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&common);
    bool repaired=false; config_=store_.load(repaired);
    config_.autostart=readAutostart();
    if (repaired) { std::wstring message; if(!store_.save(config_,message)) notice_=message; }
    background_=CreateSolidBrush(RGB(245,247,250)); white_=CreateSolidBrush(RGB(255,255,255));
    activeIcon_=static_cast<HICON>(LoadImageW(instance_,MAKEINTRESOURCEW(101),IMAGE_ICON,0,0,LR_DEFAULTSIZE));
    pausedIcon_=static_cast<HICON>(LoadImageW(instance_,MAKEINTRESOURCEW(102),IMAGE_ICON,0,0,LR_DEFAULTSIZE));
    if(!activeIcon_) activeIcon_=CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));
    if(!pausedIcon_) pausedIcon_=CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance_; wc.lpfnWndProc=windowProc;
    wc.lpszClassName=windowClass; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=activeIcon_; wc.hIconSm=activeIcon_;
    if(!RegisterClassExW(&wc)) { MessageBoxW(nullptr,L"无法注册设置窗口。",L"MouseSide",MB_OK|MB_ICONERROR); return false; }
    dpi_=GetDpiForSystem();
    RECT rect{0,0,scale(clientWidth),scale(clientHeight)};
    constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    AdjustWindowRectExForDpi(&rect,style,FALSE,0,dpi_);
    window_=CreateWindowExW(WS_EX_CONTROLPARENT,windowClass,L"MouseSide",style,CW_USEDEFAULT,CW_USEDEFAULT,
                           rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance_,this);
    if(!window_) { MessageBoxW(nullptr,L"无法创建设置窗口。",L"MouseSide",MB_OK|MB_ICONERROR); return false; }
    dpi_=GetDpiForWindow(window_);
    const BOOL dark=FALSE;
    DwmSetWindowAttribute(window_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    const DWM_WINDOW_CORNER_PREFERENCE corner=DWMWCP_ROUND;
    DwmSetWindowAttribute(window_,DWMWA_WINDOW_CORNER_PREFERENCE,&corner,sizeof(corner));
    auto child=[&](const wchar_t* type,const wchar_t* text,DWORD flags,int id) {
        HWND control=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|flags,0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
        SetWindowTheme(control,L"Explorer",nullptr);
        return control;
    };
    for(unsigned i=0;i<2;++i) {
        combos_[i]=child(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,100+static_cast<int>(i));
        for(const auto& preset : presets) SendMessageW(combos_[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(preset.name));
        SendMessageW(combos_[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"自定义快捷键…"));
        SendMessageW(combos_[i],CB_SETMINVISIBLE,11,0);
        recordButtons_[i]=child(WC_BUTTONW,L"录制快捷键",BS_PUSHBUTTON,200+static_cast<int>(i));
    }
    enabledBox_=child(WC_BUTTONW,L"启用侧键映射",BS_AUTOCHECKBOX,300);
    startupBox_=child(WC_BUTTONW,L"登录 Windows 时自动启动",BS_AUTOCHECKBOX,301);
    resetButton_=child(WC_BUTTONW,L"恢复默认",BS_PUSHBUTTON,302);
    if(!combos_[0] || !combos_[1] || !recordButtons_[0] || !recordButtons_[1] || !enabledBox_ || !startupBox_ || !resetButton_) {
        error(L"无法创建设置控件。请重新启动 MouseSide。"); return false;
    }
    layout(); taskbarCreated_=RegisterWindowMessageW(L"TaskbarCreated");
    const bool trayOk=addTray();
    DWORD hookError=0; hookReady_=input_.start(window_,config_,hookError);
    if(!hookReady_) { config_.enabled=false; error(L"鼠标监听初始化失败，侧键保留原始功能。\n"+systemError(hookError)); }
    refresh();
    if(!backgroundStart_ || !trayOk || !hookReady_ || !notice_.empty()) show();
    if(!trayOk) error(L"系统托盘初始化失败。设置窗口会保持可见，可从窗口菜单退出程序。");
    if(!notice_.empty()) error(notice_);
    return true;
}
void App::layout() {
    for(HFONT font : {titleFont_,bodyFont_,smallFont_,boldFont_,keyFont_}) if(font) DeleteObject(font);
    titleFont_=makeFont(25,FW_BOLD); bodyFont_=makeFont(13,FW_NORMAL); smallFont_=makeFont(11,FW_NORMAL);
    boldFont_=makeFont(14,FW_SEMIBOLD); keyFont_=makeFont(12,FW_SEMIBOLD);
    auto position=[&](HWND control,int x,int y,int width,int height) {
        SetWindowPos(control,nullptr,scale(x),scale(y),scale(width),scale(height),SWP_NOZORDER|SWP_NOACTIVATE);
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont_),TRUE);
    };
    for(unsigned i=0;i<2;++i) {
        const int y=204+static_cast<int>(i)*146;
        position(combos_[i],42,y,296,320);
        SendMessageW(combos_[i],CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),scale(26));
        SendMessageW(combos_[i],CB_SETITEMHEIGHT,0,scale(28));
        position(recordButtons_[i],354,y,152,34);
    }
    position(enabledBox_,42,444,300,26); position(startupBox_,42,495,390,26); position(resetButton_,26,559,116,34);
    InvalidateRect(window_,nullptr,FALSE);
}
void App::drawText(HDC dc,const std::wstring& text,int x,int y,int width,int height,HFONT font,COLORREF color,UINT flags) {
    const auto old=SelectObject(dc,font); SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    RECT rect{scale(x),scale(y),scale(x+width),scale(y+height)};
    DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&rect,flags|DT_END_ELLIPSIS|DT_NOPREFIX);
    SelectObject(dc,old);
}
void App::card(HDC dc,int x,int y,int width,int height) {
    const auto oldBrush=SelectObject(dc,white_);
    HPEN pen=CreatePen(PS_SOLID,1,RGB(228,232,239)); const auto oldPen=SelectObject(dc,pen);
    RoundRect(dc,scale(x),scale(y),scale(x+width),scale(y+height),scale(16),scale(16));
    SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(pen);
}
void App::paint() {
    PAINTSTRUCT ps{}; HDC target=BeginPaint(window_,&ps);
    RECT client; GetClientRect(window_,&client);
    HDC dc=CreateCompatibleDC(target); HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);
    const auto oldBitmap=SelectObject(dc,bitmap); FillRect(dc,&client,background_);
    DrawIconEx(dc,scale(26),scale(29),activeIcon_,scale(42),scale(42),0,nullptr,DI_NORMAL);
    drawText(dc,L"MouseSide",82,20,250,38,titleFont_,ink);
    drawText(dc,L"让两个侧键，刚好顺手。",83,61,270,19,smallFont_,muted);
    HBRUSH pill=CreateSolidBrush(config_.enabled && hookReady_ ? RGB(229,240,255) : RGB(230,234,240));
    auto oldBrush=SelectObject(dc,pill); auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,scale(410),scale(34),scale(522),scale(64),scale(30),scale(30));
    SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(pill);
    drawText(dc,recording_>=0 ? L"正在录制" : config_.enabled && hookReady_ ? L"映射已启用" : L"映射已暂停",410,34,112,30,smallFont_,blue,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    drawText(dc,L"按键映射",26,99,150,22,boldFont_,ink);
    drawText(dc,L"更改后立即生效",362,101,160,20,smallFont_,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    for(unsigned i=0;i<2;++i) {
        const int y=130+static_cast<int>(i)*146;
        card(dc,26,y,496,132);
        HBRUSH badge=CreateSolidBrush(RGB(237,244,255)); oldBrush=SelectObject(dc,badge); oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
        RoundRect(dc,scale(42),scale(y+19),scale(80),scale(y+57),scale(12),scale(12));
        SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(badge);
        drawText(dc,std::to_wstring(i+4),42,y+19,38,38,boldFont_,blue,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        drawText(dc,i==0 ? L"第四按钮" : L"第五按钮",94,y+14,155,28,boldFont_,ink);
        drawText(dc,i==0 ? L"XBUTTON1" : L"XBUTTON2",94,y+43,130,18,smallFont_,muted);
        const auto action=config_.buttons[i];
        const std::wstring key=recording_==static_cast<int>(i) ? L"请按下组合键…" : action.kind==ActionKind::Shortcut ? shortcutName(action.shortcut) : action.kind==ActionKind::Original ? L"Windows 默认行为" : L"不执行操作";
        drawText(dc,key,248,y+23,258,30,keyFont_,recording_==static_cast<int>(i) ? blue : muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    }
    card(dc,26,430,496,112);
    drawText(dc,L"暂停后，侧键恢复 Windows 原始功能",65,471,400,18,smallFont_,muted);
    drawText(dc,L"启动后安静驻留托盘",65,522,400,16,smallFont_,muted);
    drawText(dc,recording_>=0 ? L"按 Esc 取消，松开所有按键后保存" : L"更改自动保存 · 关闭窗口后继续在托盘运行",157,559,365,34,smallFont_,muted);
    drawText(dc,L"V1.0.0  /  Codex",26,602,160,16,smallFont_,muted);
    drawText(dc,L"原生 · 离线 · 轻量",300,602,222,16,smallFont_,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); EndPaint(window_,&ps);
}
void App::refresh() {
    for(unsigned i=0;i<2;++i) {
        int selection=10;
        for(unsigned j=0;j<presets.size();++j) if(config_.buttons[i]==presets[j].action) selection=static_cast<int>(j);
        SendMessageW(combos_[i],CB_SETCURSEL,static_cast<WPARAM>(selection),0);
        EnableWindow(combos_[i],recording_<0);
        EnableWindow(recordButtons_[i],recording_<0 || recording_==static_cast<int>(i));
        SetWindowTextW(recordButtons_[i],recording_==static_cast<int>(i) ? L"取消录制" : L"录制快捷键");
        SetWindowTextW(combos_[i],i==0 ? L"第四按钮映射" : L"第五按钮映射");
    }
    SendMessageW(enabledBox_,BM_SETCHECK,config_.enabled ? BST_CHECKED : BST_UNCHECKED,0);
    SendMessageW(startupBox_,BM_SETCHECK,config_.autostart ? BST_CHECKED : BST_UNCHECKED,0);
    EnableWindow(enabledBox_,recording_<0); EnableWindow(startupBox_,recording_<0); EnableWindow(resetButton_,recording_<0);
    updateTray(); InvalidateRect(window_,nullptr,FALSE);
}
void App::error(const std::wstring& text) {
    show(); MessageBoxW(window_,text.c_str(),L"MouseSide",MB_OK|MB_ICONERROR);
}
bool App::apply(Config next) {
    std::wstring message;
    if(!store_.save(next,message)) { error(message+L"\n设置未更改。"); refresh(); return false; }
    config_=next; input_.update(config_); refresh(); return true;
}
void App::setEnabled(bool enabled) {
    if(recording_>=0) return;
    if(enabled && !hookReady_) {
        DWORD code=0; hookReady_=input_.start(window_,config_,code);
        if(!hookReady_) { error(L"无法启用鼠标监听："+systemError(code)); refresh(); return; }
    }
    Config next=config_; next.enabled=enabled; apply(next);
}
void App::beginRecording(unsigned button) {
    if(recording_>=0) { recorder_.cancel(); return; }
    DWORD code=0;
    if(!recorder_.begin(window_,code)) { error(code==ERROR_BUSY ? L"请先松开所有键盘按键，再开始录制。" : L"快捷键录制初始化失败："+systemError(code)); refresh(); return; }
    recording_=static_cast<int>(button);
    Config paused=config_; paused.enabled=false; input_.update(paused); refresh();
}
void App::finishRecording(unsigned result) {
    if(recording_<0) return;
    const int button=recording_; recording_=-1; recorder_.finish();
    if(result) { Config next=config_; next.buttons[static_cast<unsigned>(button)]=unpackAction(result); if(!apply(next)) input_.update(config_); }
    else { input_.update(config_); refresh(); }
}
void App::reset() {
    if(MessageBoxW(window_,L"恢复默认映射（第四按钮粘贴、第五按钮复制），启用映射并关闭开机自启？",L"恢复默认",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
    std::wstring message;
    const bool previousStartup=config_.autostart;
    if(!writeAutostart(false,message)) { error(message); return; }
    if(!apply(Config{})) {
        if(!writeAutostart(previousStartup,message)) { config_.autostart=readAutostart(); refresh(); error(message); }
    }
    if(!hookReady_) setEnabled(true);
}
void App::show() {
    ShowWindow(window_,IsIconic(window_) ? SW_RESTORE : SW_SHOW);
    // Explicit user wake must override a launcher's STARTF_USESHOWWINDOW/SW_HIDE.
    SetWindowPos(window_,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW);
    SetForegroundWindow(window_);
}
bool App::addTray() {
    NOTIFYICONDATAW icon{}; icon.cbSize=sizeof(icon); icon.hWnd=window_; icon.uID=1;
    icon.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP; icon.uCallbackMessage=trayMessage;
    icon.hIcon=config_.enabled ? activeIcon_ : pausedIcon_;
    wcscpy_s(icon.szTip,L"MouseSide");
    trayAdded_=Shell_NotifyIconW(NIM_ADD,&icon)!=FALSE;
    if(trayAdded_) { icon.uVersion=NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION,&icon); updateTray(); }
    return trayAdded_;
}
void App::updateTray() {
    if(!trayAdded_) return;
    NOTIFYICONDATAW icon{}; icon.cbSize=sizeof(icon); icon.hWnd=window_; icon.uID=1; icon.uFlags=NIF_ICON|NIF_TIP;
    const bool enabled=config_.enabled && hookReady_ && recording_<0;
    icon.hIcon=enabled ? activeIcon_ : pausedIcon_;
    wcscpy_s(icon.szTip,enabled ? L"MouseSide · 映射已启用" : L"MouseSide · 映射已暂停");
    Shell_NotifyIconW(NIM_MODIFY,&icon);
}
void App::trayMenu() {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,openCommand,L"打开设置");
    AppendMenuW(menu,MF_STRING|(recording_>=0 ? MF_GRAYED : 0),toggleCommand,config_.enabled ? L"暂停映射" : L"启用映射");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr); AppendMenuW(menu,MF_STRING,exitCommand,L"退出程序");
    POINT point{}; GetCursorPos(&point); SetForegroundWindow(window_);
    const auto command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window_,nullptr);
    DestroyMenu(menu); PostMessageW(window_,WM_NULL,0,0);
    if(command) SendMessageW(window_,WM_COMMAND,command,0);
}
LRESULT CALLBACK App::windowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); app->window_=window;
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->handle(message,w,l) : DefWindowProcW(window,message,w,l);
}
LRESULT App::handle(UINT message,WPARAM w,LPARAM l) {
    if(taskbarCreated_ && message==taskbarCreated_) { trayAdded_=false; if(!addTray()) { show(); error(L"无法恢复托盘图标。请重新启动 MouseSide。"); } return 0; }
    switch(message) {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: { auto dc=reinterpret_cast<HDC>(w); SetBkColor(dc,RGB(255,255,255)); SetTextColor(dc,ink); return reinterpret_cast<LRESULT>(white_); }
    case WM_DPICHANGED: {
        dpi_=HIWORD(w); const auto* rect=reinterpret_cast<RECT*>(l);
        SetWindowPos(window_,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
        layout(); return 0;
    }
    case WM_CLOSE:
        if(recording_>=0) recorder_.cancel();
        if(trayAdded_) ShowWindow(window_,SW_HIDE);
        else if(MessageBoxW(window_,L"托盘不可用。退出 MouseSide？",L"MouseSide",MB_YESNO|MB_ICONQUESTION)==IDYES) DestroyWindow(window_);
        return 0;
    case WM_SYSCOMMAND:
        if((w & 0xFFF0u)==SC_MINIMIZE && trayAdded_) { SendMessageW(window_,WM_CLOSE,0,0); return 0; }
        break;
    case WM_ACTIVATE: if(LOWORD(w)==WA_INACTIVE && recording_>=0) recorder_.cancel(); break;
    case WM_POWERBROADCAST:
        if(w==PBT_APMSUSPEND) { input_.stop(); hookReady_=false; }
        if(w==PBT_APMRESUMEAUTOMATIC || w==PBT_APMRESUMESUSPEND) {
            DWORD code=0; hookReady_=input_.start(window_,config_,code);
            if(!hookReady_) { Config paused=config_; paused.enabled=false; apply(paused); error(L"唤醒后无法恢复鼠标监听："+systemError(code)); }
        }
        refresh(); return TRUE;
    case showMessage: show(); return 0;
    case inputErrorMessage: {
        Config paused=config_; paused.enabled=false;
        if(!apply(paused)) { config_.enabled=false; input_.update(config_); refresh(); }
        error(L"快捷键发送失败，映射已暂停。高权限窗口或系统安全桌面可能限制输入模拟。\n"+systemError(static_cast<DWORD>(w))); return 0;
    }
    case recordedMessage:
        finishRecording(static_cast<unsigned>(w));
        if(l) error(L"此组合键不支持录制。请选择常规按键；系统安全快捷键不可映射。");
        return 0;
    case trayMessage:
        if(LOWORD(l)==WM_LBUTTONDBLCLK || LOWORD(l)==NIN_KEYSELECT) show();
        else if(LOWORD(l)==WM_CONTEXTMENU) trayMenu();
        return 0;
    case WM_COMMAND: {
        const unsigned id=LOWORD(w);
        if(id==openCommand) show();
        else if(id==toggleCommand) setEnabled(!config_.enabled);
        else if(id==exitCommand) DestroyWindow(window_);
        else if((id==100 || id==101) && HIWORD(w)==CBN_SELCHANGE && recording_<0) {
            const auto selection=SendMessageW(combos_[id-100],CB_GETCURSEL,0,0);
            if(selection==10) beginRecording(id-100);
            else if(selection>=0 && selection<static_cast<LRESULT>(presets.size())) { Config next=config_; next.buttons[id-100]=presets[static_cast<unsigned>(selection)].action; apply(next); }
        } else if(id==200 || id==201) beginRecording(id-200);
        else if(id==300) setEnabled(SendMessageW(enabledBox_,BM_GETCHECK,0,0)==BST_CHECKED);
        else if(id==301) {
            const bool enabled=SendMessageW(startupBox_,BM_GETCHECK,0,0)==BST_CHECKED;
            std::wstring message;
            if(!writeAutostart(enabled,message)) { error(message); refresh(); }
            else { const bool previous=config_.autostart; Config next=config_; next.autostart=enabled; if(!apply(next) && !writeAutostart(previous,message)) { config_.autostart=readAutostart(); refresh(); error(message); } }
        } else if(id==302) reset();
        return 0;
    }
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if(w) DestroyWindow(window_); return 0;
    case WM_DESTROY:
        recorder_.finish(); input_.stop();
        if(trayAdded_) { NOTIFYICONDATAW icon{}; icon.cbSize=sizeof(icon); icon.hWnd=window_; icon.uID=1; Shell_NotifyIconW(NIM_DELETE,&icon); trayAdded_=false; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window_,message,w,l);
}
}
