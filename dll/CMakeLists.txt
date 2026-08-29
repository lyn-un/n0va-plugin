# n0va_plugin.dll — 注入核心（PLAN.md 组件一）
# 关键构建要求（PLAN.md 3.2）: /MD 动态 CRT（根 CMakeLists 已全局设置）
#
# ★ 设计说明（qt_bridge.cpp 头部有完整理由）:
#   最终方案不链接 Qt5Core——主线程桥改为 Detours hook
#   QEventDispatcherWin32::sendPostedEvents（Qt 导出, GetProcAddress 动态解析）。
#   因此这里没有 /DELAYLOAD:Qt5Core.dll（那是对"继承 QObject"方案的残留）。
add_library(n0va_plugin SHARED
    dllmain.cpp
    bootstrap.cpp
    logging.cpp
    qt_bridge.cpp
    injector.cpp
    pipe_server.cpp
    injected_store.cpp
)

target_include_directories(n0va_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(n0va_plugin PRIVATE n0va_core)

if(MSVC)
    target_compile_definitions(n0va_plugin PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN
    )
endif()

# ★ 诊断: 生成 MAP 文件（崩溃栈偏移 → 函数名映射, 发布版可去掉）
if(MSVC)
    target_link_options(n0va_plugin PRIVATE "/MAP:${CMAKE_BINARY_DIR}/n0va_plugin.map" "/MAPINFO:EXPORTS")
endif()
