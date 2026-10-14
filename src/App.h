#pragma once
#include "Config.h"
#include "Input.h"
#include <shellapi.h>
namespace ms {
inline constexpr const wchar_t* windowClass=L"MouseSide.Codex.Settings";
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();
    int run(bool background);
private:
    static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM w,LPARAM l);
    LRESULT handle(UINT message,WPARAM w,LPARAM l);
    bool initialize();
    void layout();
    void paint();
    void refresh();
    void show();
    void trayMenu();
    bool addTray();
    void updateTray();
    void error(const std::wstring& text);
    bool apply(Config next);
    void beginRecording(unsigned button);
    void finishRecording(unsigned result);
    void setEnabled(bool enabled);
    void reset();
    int scale(int value) const { return MulDiv(value,static_cast<int>(dpi_),96); }
    HFONT makeFont(int size,int weight);
    void drawText(HDC dc,const std::wstring& text,int x,int y,int width,int height,HFONT font,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    void card(HDC dc,int x,int y,int width,int height);
    HINSTANCE instance_;
    HWND window_=nullptr;
    ConfigStore store_;
    Config config_;
    InputEngine input_;
    ShortcutRecorder recorder_;
    std::array<HWND,2> combos_{};
    std::array<HWND,2> recordButtons_{};
    HWND enabledBox_=nullptr, startupBox_=nullptr, resetButton_=nullptr;
    HFONT titleFont_=nullptr, bodyFont_=nullptr, smallFont_=nullptr, boldFont_=nullptr, keyFont_=nullptr;
    HBRUSH background_=nullptr, white_=nullptr;
    HICON activeIcon_=nullptr, pausedIcon_=nullptr;
    UINT dpi_=96, taskbarCreated_=0;
    bool trayAdded_=false, hookReady_=false, backgroundStart_=false;
    int recording_=-1;
    std::wstring notice_;
};
}
