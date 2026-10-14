#pragma once
// 仅在 MSIDE_DEBUG 构建时输出诊断日志到 %TEMP%\mouseside_debug.log
#ifdef MSIDE_DEBUG

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>

inline void MsDbg(const char* fmt, ...) {
    static FILE* f = nullptr;
    if (!f) {
        char path[MAX_PATH] = {0};
        GetEnvironmentVariableA("TEMP", path, MAX_PATH);
        strncat(path, "\\mouseside_debug.log", MAX_PATH - strlen(path) - 1);
        f = fopen(path, "a");
        if (!f) return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

#else

inline void MsDbg(const char*, ...) {}

#endif
