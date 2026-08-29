# core.lib — CLI/DLL 共用核心库（PLAN.md 组件二）
add_library(n0va_core STATIC
    dll_inject.cpp
    pipe_client.cpp
    wallpaper_db.cpp
    fs_helper.cpp
    plugin_settings.cpp
)

target_include_directories(n0va_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(n0va_core PUBLIC detours)

if(MSVC)
    target_compile_definitions(n0va_core PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
