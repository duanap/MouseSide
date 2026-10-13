@echo off
setlocal

rem MouseSide - MSVC Release 构建脚本（需安装 Visual Studio 2022 与 CMake）
rem 用法：双击运行，或在命令行执行 build_msvc.bat

where cmake >nul 2>nul
if errorlevel 1 (
    echo 未找到 cmake，请先安装 CMake 并添加到 PATH。
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build --config Release
if errorlevel 1 exit /b 1

echo.
echo 构建完成：build\Release\MouseSide.exe
endlocal
