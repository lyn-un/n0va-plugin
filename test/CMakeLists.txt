# 离线单元测试（默认关; cmake -DBUILD_TESTS=ON 启用）
add_executable(test_dll_inject test_dll_inject.cpp)
target_link_libraries(test_dll_inject PRIVATE n0va_core)

add_executable(test_pipe test_pipe.cpp)
target_link_libraries(test_pipe PRIVATE n0va_core)

add_executable(test_firstframe test_firstframe.cpp)
target_link_libraries(test_firstframe PRIVATE n0va_core)

add_executable(test_dimensions test_dimensions.cpp)
target_link_libraries(test_dimensions PRIVATE n0va_core)

foreach(t test_dll_inject test_pipe test_firstframe test_dimensions)
    if(MSVC)
        target_compile_definitions(${t} PRIVATE
            _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    endif()
endforeach()
