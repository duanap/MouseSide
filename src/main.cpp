#include "App.h"
#include <memory>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\MouseSide.Codex.V1.Singleton");
    if (!singleton) { MessageBoxW(nullptr,L"无法建立单实例保护。",L"MouseSide",MB_OK|MB_ICONERROR); return 1; }
    if (GetLastError()==ERROR_ALREADY_EXISTS) {
        HWND existing=nullptr;
        for (int attempt=0; attempt<60 && !existing; ++attempt) { existing=FindWindowW(ms::windowClass,nullptr); if (!existing) Sleep(50); }
        if (existing) {
            DWORD pid=0; GetWindowThreadProcessId(existing,&pid); AllowSetForegroundWindow(pid);
            PostMessageW(existing,WM_APP+11,0,0);
        } else MessageBoxW(nullptr,L"MouseSide 正在启动，请稍后再打开。",L"MouseSide",MB_OK|MB_ICONINFORMATION);
        CloseHandle(singleton); return existing ? 0 : 1;
    }
    int result=1;
    try {
        int argc=0; PWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        bool background=false;
        if (argv) { for(int i=1;i<argc;++i) if(wcscmp(argv[i],L"--background")==0) background=true; LocalFree(argv); }
        ms::App app(instance); result=app.run(background);
    } catch (const std::exception&) {
        MessageBoxW(nullptr,L"程序初始化失败。请检查本地配置目录的访问权限。",L"MouseSide",MB_OK|MB_ICONERROR);
    }
    CloseHandle(singleton); return result;
}
