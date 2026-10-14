#include "Test.h"
#include "Config.h"
#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
struct ConfigBackup {
    std::filesystem::path path=ms::ConfigStore::defaultPath();
    bool existed=std::filesystem::exists(path);
    std::string bytes;
    ConfigBackup() { if(existed) { std::ifstream file(path,std::ios::binary); bytes.assign(std::istreambuf_iterator<char>(file),{}); } }
    ~ConfigBackup() { if(existed) { std::ofstream file(path,std::ios::binary|std::ios::trunc); file.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); } else { std::error_code ec; std::filesystem::remove(path,ec); } }
};
static PROCESS_INFORMATION launch(const std::wstring& exe,const wchar_t* args) {
    STARTUPINFOW startup{}; startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    std::wstring command=L"\""+exe+L"\" "+args;
    CHECK(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));
    CloseHandle(process.hThread);
    return process;
}
int wmain(int argc,wchar_t** argv) {
    HANDLE first=nullptr;
    try {
        CHECK(argc==2);
        CHECK(!FindWindowW(L"MouseSide.Codex.Settings",nullptr));
        ConfigBackup configBackup;
        auto process=launch(argv[1],L"--background"); first=process.hProcess;
        HWND window=nullptr;
        for (int i=0;i<100 && !window;++i) { Sleep(50); window=FindWindowW(L"MouseSide.Codex.Settings",nullptr); }
        CHECK(window);
        CHECK(!IsWindowVisible(window));
        auto second=launch(argv[1],L"");
        CHECK(WaitForSingleObject(second.hProcess,5000)==WAIT_OBJECT_0);
        DWORD code=1; GetExitCodeProcess(second.hProcess,&code); CloseHandle(second.hProcess);
        CHECK(code==0);
        for (int i=0;i<40 && !IsWindowVisible(window);++i) Sleep(50);
        CHECK(IsWindowVisible(window));
        auto combo=GetDlgItem(window,100);
        CHECK(combo);
        SendMessageW(combo,CB_SETCURSEL,2,0); // cut
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(100,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
        ms::ConfigStore store(ms::ConfigStore::defaultPath());
        bool repaired=false;
        CHECK(store.load(repaired).buttons[0].shortcut == ms::Shortcut{'X',ms::Ctrl});
        CHECK(!repaired);
        const bool enabledBefore=store.load(repaired).enabled;
        HWND toggle=GetDlgItem(window,300);
        CHECK(toggle);
        SendMessageW(window,WM_NEXTDLGCTL,reinterpret_cast<WPARAM>(toggle),TRUE);
        SendMessageW(toggle,WM_KEYDOWN,VK_SPACE,0);
        SendMessageW(toggle,WM_KEYUP,VK_SPACE,0);
        CHECK(store.load(repaired).enabled == !enabledBefore);
        SendMessageW(toggle,BM_CLICK,0,0);
        CHECK(store.load(repaired).enabled == enabledBefore);
        SendMessageW(combo,CB_SETCURSEL,1,0); // restore default for visual checks
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(100,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
        SendMessageW(window,WM_CLOSE,0,0);
        CHECK(!IsWindowVisible(window));
        CHECK(WaitForSingleObject(first,0)==WAIT_TIMEOUT);
        PostMessageW(window,WM_COMMAND,9003,0); // tray Exit command
        CHECK(WaitForSingleObject(first,5000)==WAIT_OBJECT_0);
        CloseHandle(first); first=nullptr;
        CHECK(!FindWindowW(L"MouseSide.Codex.Settings",nullptr));
        std::cout << "PASS: background start, single-instance wake, preset persistence, keyboard/pointer toggle, close-to-tray, clean exit\n";
        return 0;
    } catch(const std::exception& e) {
        if(first) { auto window=FindWindowW(L"MouseSide.Codex.Settings",nullptr); if(window) PostMessageW(window,WM_COMMAND,9003,0); WaitForSingleObject(first,5000); CloseHandle(first); }
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
}
