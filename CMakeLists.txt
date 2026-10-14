cmake_minimum_required(VERSION 3.20)

project(MouseSide
    VERSION 1.0.0
    DESCRIPTION "Windows lightweight mouse side-button mapper"
    LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

# 单配置生成器（Ninja / NMake）默认使用 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MOUSESIDE_SOURCES
    src/main.cpp
    src/App.cpp
    src/Config.cpp
    src/IconFactory.cpp
    src/KeyMapper.cpp
    src/MouseHook.cpp
    src/HotkeyRecorder.cpp
    src/TrayUI.cpp
    assets/resources.rc
)

add_executable(MouseSide WIN32 ${MOUSESIDE_SOURCES})

# 静态资源（.rc）内部按文件名引用 app.manifest / MouseSide.ico
target_include_directories(MouseSide PRIVATE src assets)

target_compile_definitions(MouseSide PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

target_link_libraries(MouseSide PRIVATE user32 shell32 gdi32 advapi32 comctl32)

if(MSVC)
    target_compile_options(MouseSide PRIVATE /utf-8 /W4 /permissive- /EHsc)
    target_compile_options(MouseSide PRIVATE
        "$<$<CONFIG:Release>:/O2>"
        "$<$<CONFIG:Release>:/GL>"
        "$<$<CONFIG:Release>:/Gy>"
    )
    target_link_options(MouseSide PRIVATE
        "$<$<CONFIG:Release>:/LTCG>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>"
    )
    # 静态链接 CRT：生成的 EXE 免安装、无 VC++ 运行库依赖
    set_target_properties(MouseSide PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # MinGW-w64 兜底（非官方目标平台，仅用于语法/链接自检）
    target_compile_options(MouseSide PRIVATE -Wall -Wextra)
    target_link_options(MouseSide PRIVATE -municode -mwindows -static -s)
endif()

# 输出到 build/bin，方便直接取 Release EXE
set_target_properties(MouseSide PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(MouseSide PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin")
endforeach()
