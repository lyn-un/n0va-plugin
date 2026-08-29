cmake_minimum_required(VERSION 3.20)
project(n0va_wallpaper_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VS2022 多线程 DLL CRT (/MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_compile_options(/W3 /utf-8)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------- Microsoft Detours (静态库) ----------
# 直接编译 Detours 源文件, 避免依赖其 nmake Makefile
# 注意: uimports.cpp 是"被 creatwth.cpp 以宏包含多次"的模板文件（文件头注释说明）,
#       不能作为独立编译单元; 其余 disol*.cpp 亦为模板, 由 disasm.cpp 包含。
add_library(detours STATIC
    third_party/Detours/src/creatwth.cpp
    third_party/Detours/src/detours.cpp
    third_party/Detours/src/disasm.cpp
    third_party/Detours/src/image.cpp
    third_party/Detours/src/modules.cpp
)
target_include_directories(detours PUBLIC third_party/Detours/src)
target_compile_definitions(detours PRIVATE
    DETOURS_X64=1
    _CRT_SECURE_NO_WARNINGS
    DETOURS_VERSION=0x4c0c1)  # detver.h 要求(与 detours.h 版本宏匹配, 否则 uimports.cpp 报 mismatch)
if(MSVC)
    target_compile_options(detours PRIVATE /W0)  # 第三方代码不告警
endif()

# ---------- core 静态库 ----------
add_subdirectory(core)

# ---------- n0va_plugin.dll ----------
add_subdirectory(dll)

# ---------- CLI ----------
add_subdirectory(cli)

# ---------- 测试（默认关） ----------
option(BUILD_TESTS "Build offline unit tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(test)
endif()
