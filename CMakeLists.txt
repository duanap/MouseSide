cmake_minimum_required(VERSION 3.20)
project(MouseSide VERSION 1.0.2 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# MSVC：静态链接运行时，避免依赖 VC 运行库（单文件、免安装）
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(MOUSESIDE_SOURCES
    src/main.cpp
    src/Config.cpp
    src/KeyMapper.cpp
    src/MouseHook.cpp
    src/SettingsWindow.cpp
    src/TrayUI.cpp
    src/app.rc
)

set(MOUSESIDE_HEADERS
    src/Common.h
    src/Config.h
    src/KeyMapper.h
    src/MouseHook.h
    src/SettingsWindow.h
    src/TrayUI.h
    src/resource.h
    src/app.manifest
)

add_executable(MouseSide WIN32 ${MOUSESIDE_SOURCES} ${MOUSESIDE_HEADERS})

target_include_directories(MouseSide PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/res
)

target_compile_definitions(MouseSide PRIVATE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    UNICODE
    _UNICODE
)

target_link_libraries(MouseSide PRIVATE
    user32
    gdi32
    shell32
    ole32
    advapi32
    comctl32
    shlwapi
)

if(MSVC)
    target_compile_options(MouseSide PRIVATE /utf-8 /W4 /permissive- /Zc:__cplusplus)
    target_compile_options(MouseSide PRIVATE
        $<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/Gy> $<$<CONFIG:Release>:/GF>)
    target_link_options(MouseSide PRIVATE
        $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
else()
    target_compile_options(MouseSide PRIVATE -Wall -Wextra -municode)
    target_link_options(MouseSide PRIVATE -municode)
    if(MINGW)
        # 静态链接，单文件 EXE 无需额外 DLL
        target_link_options(MouseSide PRIVATE -static -static-libgcc -static-libstdc++)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_link_options(MouseSide PRIVATE -s)
        endif()
    endif()
endif()

set_target_properties(MouseSide PROPERTIES
    OUTPUT_NAME "MouseSide"
    WIN32_EXECUTABLE TRUE
)

# 安装：默认把可执行文件放到 install/bin
install(TARGETS MouseSide RUNTIME DESTINATION bin)

# Regression tests use the real source and isolate desktop/input OS boundaries.
include(CTest)
if(BUILD_TESTING)
    add_executable(MouseSideRegression tests/regression.cpp src/app.rc)
    target_include_directories(MouseSideRegression PRIVATE src res)
    target_compile_definitions(MouseSideRegression PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_link_libraries(MouseSideRegression PRIVATE user32 gdi32 shell32 ole32 advapi32 comctl32 shlwapi)
    if(MSVC)
        target_compile_options(MouseSideRegression PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(MouseSideRegression PRIVATE -Wall -Wextra)
        target_link_options(MouseSideRegression PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME MouseSideRegression COMMAND MouseSideRegression)
endif()
