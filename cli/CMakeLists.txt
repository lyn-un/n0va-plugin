# n0va_plugin.exe — 命令行工具（PLAN.md 组件三）
# 目标名与 DLL 区分; 输出名仍为 n0va_plugin.exe
add_executable(n0va_plugin_cli
    main.cpp
    cmd_install.cpp
    cmd_launch.cpp
    cmd_inject.cpp
    cmd_list.cpp
    cmd_remove.cpp
    cmd_status.cpp
    cmd_doctor.cpp
    cmd_set_host.cpp
)
set_target_properties(n0va_plugin_cli PROPERTIES OUTPUT_NAME "n0va_plugin")

target_link_libraries(n0va_plugin_cli PRIVATE n0va_core bcrypt)

if(MSVC)
    target_compile_definitions(n0va_plugin_cli PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
endif()
