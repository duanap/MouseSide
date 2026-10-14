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
constexpr COLORREF ink=RGB(32,35,40), muted=RGB(117,123,132), blue=RGB(0,95,184);
constexpr int clientWidth=520, clientHeight=502;
static unsigned controlIndex(int id) { return static_cast<unsigned>(id<200 ? id-100 : id<300 ? id-198 : id-296); }
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
                      CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
}
bool App::initialize() {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&common);
    bool repaired=false; config_=store_.load(repaired);
    config_.autostart=readAutostart();
    if (repaired) { std::wstring message; if(!store_.save(config_,message)) notice_=message; }
    background_=CreateSolidBrush(RGB(248,249,251)); white_=CreateSolidBrush(RGB(255,255,255));
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
    const COLORREF captionColor=RGB(248,249,251);
    DwmSetWindowAttribute(window_,DWMWA_CAPTION_COLOR,&captionColor,sizeof(captionColor));
    auto child=[&](const wchar_t* type,const wchar_t* text,DWORD flags,int id) {
        HWND control=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|flags,0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
        SetWindowTheme(control,L"Explorer",nullptr);
        if(control) SetWindowSubclass(control,controlProc,1,reinterpret_cast<DWORD_PTR>(this));
        return control;
    };
    for(unsigned i=0;i<2;++i) {
        combos_[i]=child(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL,100+static_cast<int>(i));
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
    titleFont_=makeFont(20,FW_SEMIBOLD); bodyFont_=makeFont(13,FW_NORMAL); smallFont_=makeFont(12,FW_NORMAL);
    boldFont_=makeFont(14,FW_SEMIBOLD); keyFont_=makeFont(12,FW_NORMAL);
    auto position=[&](HWND control,int x,int y,int width,int height) {
        SetWindowPos(control,nullptr,scale(x),scale(y),scale(width),scale(height),SWP_NOZORDER|SWP_NOACTIVATE);
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont_),TRUE);
    };
    for(unsigned i=0;i<2;++i) {
        const int y=184+static_cast<int>(i)*130;
        position(combos_[i],40,y,282,350);
        SendMessageW(combos_[i],CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),scale(30));
        RECT closed{}; GetClientRect(combos_[i],&closed);
        const int chromeHeight=closed.bottom-scale(30);
        SendMessageW(combos_[i],CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),std::max(scale(20),scale(36)-chromeHeight));
        SendMessageW(combos_[i],CB_SETITEMHEIGHT,0,scale(30));
        position(recordButtons_[i],334,y,146,36);
    }
    position(enabledBox_,450,28,46,25); position(startupBox_,434,401,46,25); position(resetButton_,399,462,97,25);
    InvalidateRect(window_,nullptr,FALSE);
}
void App::drawText(HDC dc,const std::wstring& text,int x,int y,int width,int height,HFONT font,COLORREF color,UINT flags) {
    const auto old=SelectObject(dc,font);
    const auto oldColor=SetTextColor(dc,color);
    const int oldMode=SetBkMode(dc,TRANSPARENT);
    RECT rect{scale(x),scale(y),scale(x+width),scale(y+height)};
    DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&rect,flags|DT_END_ELLIPSIS|DT_NOPREFIX);
    SelectObject(dc,old);
    SetTextColor(dc,oldColor); SetBkMode(dc,oldMode);
}
void App::paint() {
    PAINTSTRUCT ps{}; HDC target=BeginPaint(window_,&ps);
    RECT client; GetClientRect(window_,&client);
    HDC dc=CreateCompatibleDC(target); HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);
    const auto oldBitmap=SelectObject(dc,bitmap); FillRect(dc,&client,background_);
    std::array<std::wstring,2> keys;
    std::array<int,2> keyWidths{};
    for(unsigned i=0;i<2;++i) {
        const auto action=config_.buttons[i];
        keys[i]=recording_==static_cast<int>(i) ? L"请按下组合键…" : action.kind==ActionKind::Shortcut ? shortcutName(action.shortcut) : action.kind==ActionKind::Original ? L"系统默认" : L"已禁用";
        const auto old=SelectObject(dc,keyFont_); SIZE size{};
        GetTextExtentPoint32W(dc,keys[i].c_str(),static_cast<int>(keys[i].size()),&size); SelectObject(dc,old);
        keyWidths[i]=std::clamp(MulDiv(size.cx,96,static_cast<int>(dpi_))+22,48,206);
    }
    {
        UiCanvas canvas(dc,dpi_);
        canvas.mouse(24,25);
        canvas.line(24,83,496,83,Gdiplus::Color(255,229,232,237),0.7F);
        for(unsigned i=0;i<2;++i) {
            const float y=116.0F+static_cast<float>(i)*130.0F;
            canvas.rounded(24,y+1,472,116,10,Gdiplus::Color(255,237,239,243));
            canvas.rounded(24,y,472,116,10,Gdiplus::Color(255,255,255,255),Gdiplus::Color(255,224,228,234));
            canvas.rounded(40,y+18,28,28,8,Gdiplus::Color(255,235,242,250));
            canvas.rounded(static_cast<float>(480-keyWidths[i]),y+18,static_cast<float>(keyWidths[i]),27,5,
                           Gdiplus::Color(255,249,250,252),Gdiplus::Color(255,232,235,240));
        }
        canvas.rounded(24,382,472,66,10,Gdiplus::Color(255,255,255,255),Gdiplus::Color(255,224,228,234));
        canvas.rounded(40,401,28,28,8,Gdiplus::Color(255,247,248,250));
        canvas.power(45,406,Gdiplus::Color(255,104,112,124));
    }
    drawText(dc,L"MouseSide",68,19,133,30,titleFont_,ink);
    drawText(dc,L"v1.0",184,24,50,22,smallFont_,muted);
    drawText(dc,L"两个侧键，按你的习惯",69,49,280,21,smallFont_,muted);
    drawText(dc,recording_>=0 ? L"录制中" : config_.enabled && hookReady_ ? L"映射已启用" : L"映射已暂停",343,28,94,25,smallFont_,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    drawText(dc,L"侧键操作",24,94,150,19,smallFont_,ink);
    drawText(dc,L"更改即时生效",354,94,142,19,smallFont_,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    for(unsigned i=0;i<2;++i) {
        const int y=116+static_cast<int>(i)*130;
        drawText(dc,std::to_wstring(i+4),40,y+18,28,28,boldFont_,blue,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        drawText(dc,i==0 ? L"第四按钮" : L"第五按钮",80,y+16,160,24,boldFont_,ink);
        drawText(dc,recording_==static_cast<int>(i) ? L"松开所有按键后保存" : L"选择操作或录制组合键",80,y+40,290,20,smallFont_,muted);
        drawText(dc,keys[i],480-keyWidths[i],y+18,keyWidths[i],27,keyFont_,recording_==static_cast<int>(i) ? blue : RGB(81,87,97),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    drawText(dc,L"开机自动启动",80,391,275,23,boldFont_,ink);
    drawText(dc,L"登录后直接在托盘运行",80,416,320,20,smallFont_,muted);
    drawText(dc,recording_>=0 ? L"按 Esc 取消录制" : L"关闭窗口后继续在托盘运行",24,466,340,20,smallFont_,muted);
    BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); EndPaint(window_,&ps);
}
void App::paintControl(HWND control,HDC dc) {
    RECT rect{}; GetClientRect(control,&rect);
    const int id=GetDlgCtrlID(control);
    const float width=static_cast<float>(rect.right)*96.0F/static_cast<float>(dpi_);
    const float height=static_cast<float>(rect.bottom)*96.0F/static_cast<float>(dpi_);
    const bool enabled=IsWindowEnabled(control)!=FALSE;
    const bool hot=enabled && hot_[controlIndex(id)];
    const bool focused=enabled && GetFocus()==control;
    FillRect(dc,&rect,id==300 || id==302 ? background_ : white_);
    if(id==300 || id==301) {
        const bool checked=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
        {
            UiCanvas canvas(dc,dpi_);
            const Gdiplus::Color fill=checked ? Gdiplus::Color(255,0,95,184) : Gdiplus::Color(255,245,246,248);
            canvas.rounded(2,2,width-4,height-4,(height-4)/2,enabled ? fill : Gdiplus::Color(255,214,218,225),
                           checked ? Gdiplus::Color(0,0,0,0) : Gdiplus::Color(255,139,146,157));
            if(hot) canvas.rounded(2,2,width-4,height-4,(height-4)/2,Gdiplus::Color(18,0,0,0));
            const float thumbSize=checked ? 15.0F : 13.0F;
            canvas.circle(checked ? width-5-thumbSize : 6,(height-thumbSize)/2,thumbSize,
                          checked ? Gdiplus::Color(255,255,255,255) : Gdiplus::Color(255,96,104,117));
            if(focused) canvas.rounded(0.5F,0.5F,width-1,height-1,4,Gdiplus::Color(0,0,0,0),Gdiplus::Color(255,0,95,184));
        }
        return;
    }
    if(id==100 || id==101) {
        const auto selection=SendMessageW(control,CB_GETCURSEL,0,0);
        std::wstring label=L"选择操作";
        if(selection>=0 && selection<static_cast<LRESULT>(presets.size())) label=presets[static_cast<unsigned>(selection)].name;
        else if(selection==10) label=L"自定义快捷键";
        {
            UiCanvas canvas(dc,dpi_);
            canvas.rounded(0.5F,0.5F,width-1,height-1,5,
                           hot ? Gdiplus::Color(255,243,246,250) : Gdiplus::Color(255,250,251,253),
                           focused ? Gdiplus::Color(255,0,95,184) : Gdiplus::Color(255,211,216,224));
            canvas.line(width-21,height/2-2,width-17,height/2+2,Gdiplus::Color(255,107,113,124));
            canvas.line(width-17,height/2+2,width-13,height/2-2,Gdiplus::Color(255,107,113,124));
        }
        drawText(dc,label,12,0,static_cast<int>(width)-45,static_cast<int>(height),bodyFont_,enabled ? ink : muted);
        return;
    }
    wchar_t label[64]{}; GetWindowTextW(control,label,64);
    const bool recording=recording_>=0 && id==200+recording_;
    const bool pressed=(SendMessageW(control,BM_GETSTATE,0,0)&BST_PUSHED)!=0;
    {
        UiCanvas canvas(dc,dpi_);
        if(id==302) {
            if(hot || pressed) canvas.rounded(0.5F,0.5F,width-1,height-1,4,Gdiplus::Color(255,236,239,244));
        } else {
            const auto fill=recording ? Gdiplus::Color(255,0,95,184) : pressed ? Gdiplus::Color(255,233,238,245) :
                            hot ? Gdiplus::Color(255,246,248,251) : Gdiplus::Color(255,255,255,255);
            canvas.rounded(0.5F,0.5F,width-1,height-1,5,fill,recording ? Gdiplus::Color(0,0,0,0) : Gdiplus::Color(255,211,216,224));
            if(!recording) canvas.pencil(13,(height-18)/2,enabled ? Gdiplus::Color(255,96,104,117) : Gdiplus::Color(255,169,175,185));
        }
        if(focused) canvas.rounded(1.5F,1.5F,width-3,height-3,4,Gdiplus::Color(0,0,0,0),Gdiplus::Color(255,0,95,184));
    }
    const auto textColor=!enabled ? muted : recording ? RGB(255,255,255) : id==302 ? muted : ink;
    drawText(dc,label,recording || id==302 ? 0 : 31,0,static_cast<int>(width)-(recording || id==302 ? 0 : 34),static_cast<int>(height),bodyFont_,textColor,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}
void App::paintComboItem(const DRAWITEMSTRUCT& item) {
    if(item.CtlType!=ODT_COMBOBOX) return;
    if(item.itemState & ODS_COMBOBOXEDIT) { InvalidateRect(item.hwndItem,nullptr,FALSE); return; }
    const bool selected=(item.itemState & ODS_SELECTED)!=0;
    HBRUSH brush=CreateSolidBrush(selected ? RGB(235,242,250) : RGB(255,255,255));
    FillRect(item.hDC,&item.rcItem,brush); DeleteObject(brush);
    std::wstring label;
    if(item.itemID<presets.size()) label=presets[item.itemID].name;
    else if(item.itemID==10) label=L"自定义快捷键…";
    RECT text=item.rcItem; text.left+=scale(12); text.right-=scale(10);
    const auto old=SelectObject(item.hDC,bodyFont_);
    const int oldMode=SetBkMode(item.hDC,TRANSPARENT);
    const auto oldColor=SetTextColor(item.hDC,selected ? blue : ink);
    DrawTextW(item.hDC,label.c_str(),static_cast<int>(label.size()),&text,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(item.hDC,old);
    SetBkMode(item.hDC,oldMode); SetTextColor(item.hDC,oldColor);
}
LRESULT CALLBACK App::controlProc(HWND control,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
    auto& app=*reinterpret_cast<App*>(data);
    const int id=GetDlgCtrlID(control);
    if(message==WM_NCDESTROY) { RemoveWindowSubclass(control,controlProc,1); return DefSubclassProc(control,message,w,l); }
    if(message==WM_PRINTCLIENT) { app.paintControl(control,reinterpret_cast<HDC>(w)); return 0; }
    if(message==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC target=BeginPaint(control,&ps); RECT rect{}; GetClientRect(control,&rect);
        if(rect.right>0 && rect.bottom>0) {
            HDC buffer=CreateCompatibleDC(target); HBITMAP bitmap=CreateCompatibleBitmap(target,rect.right,rect.bottom);
            const auto old=SelectObject(buffer,bitmap); app.paintControl(control,buffer);
            BitBlt(target,0,0,rect.right,rect.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old); DeleteObject(bitmap); DeleteDC(buffer);
        }
        EndPaint(control,&ps); return 0;
    }
    if(message==WM_ERASEBKGND) return 1;
    if(message==WM_MOUSEMOVE && !app.hot_[controlIndex(id)]) {
        app.hot_[controlIndex(id)]=true;
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0}; TrackMouseEvent(&track); InvalidateRect(control,nullptr,FALSE);
    } else if(message==WM_MOUSELEAVE) { app.hot_[controlIndex(id)]=false; InvalidateRect(control,nullptr,FALSE); }
    const auto result=DefSubclassProc(control,message,w,l);
    if(message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_ENABLE || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP ||
       message==WM_KEYDOWN || message==WM_KEYUP || message==BM_SETCHECK || message==BM_SETSTATE || message==CB_SETCURSEL || message==CB_SHOWDROPDOWN)
        InvalidateRect(control,nullptr,FALSE);
    return result;
}
void App::refresh() {
    for(unsigned i=0;i<2;++i) {
        int selection=10;
        for(unsigned j=0;j<presets.size();++j) if(config_.buttons[i]==presets[j].action) selection=static_cast<int>(j);
        SendMessageW(combos_[i],CB_SETCURSEL,static_cast<WPARAM>(selection),0);
        EnableWindow(combos_[i],recording_<0);
        EnableWindow(recordButtons_[i],recording_<0 || recording_==static_cast<int>(i));
        SetWindowTextW(recordButtons_[i],recording_==static_cast<int>(i) ? L"取消录制" : L"录制快捷键");
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
    case WM_MEASUREITEM: {
        auto* item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);
        if(item->CtlType==ODT_COMBOBOX) { item->itemHeight=static_cast<UINT>(scale(30)); return TRUE; }
        break;
    }
    case WM_DRAWITEM: paintComboItem(*reinterpret_cast<DRAWITEMSTRUCT*>(l)); return TRUE;
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
