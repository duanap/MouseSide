cmake_minimum_required(VERSION 3.15)
project(MouseSide VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MouseSide WIN32
  src/main.cpp
  src/vkmap.cpp
  src/config.cpp
  src/mapper.cpp
  src/hook.cpp
  src/tray.cpp
  src/ui.cpp
  mouseside.rc)

option(MSIDE_DEBUG "Log diagnostics to %TEMP%\\mouseside_debug.log" OFF)

target_compile_definitions(MouseSide PRIVATE
  UNICODE _UNICODE NOMINMAX
  _WIN32_WINNT=0x0A00)

if (MSIDE_DEBUG)
  target_compile_definitions(MouseSide PRIVATE MSIDE_DEBUG)
endif()

target_link_libraries(MouseSide PRIVATE
  user32 gdi32 shell32 advapi32 comctl32)

if (MSVC)
  target_compile_options(MouseSide PRIVATE /O2 /W4 /utf-8)
else()
  # MinGW：静态链接运行库，产出免依赖的独立 EXE
  target_compile_options(MouseSide PRIVATE -Os -Wall -Wextra)
  target_link_options(MouseSide PRIVATE
    -municode -static -static-libgcc -static-libstdc++ -s)
endif()
