cmake_minimum_required(VERSION 3.20)
project(mousekey LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

add_library(mkcore STATIC
    src/config.cpp
    src/engine.cpp
    src/hook.cpp
    src/inject.cpp
)
target_include_directories(mkcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(mkcore PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(mkcore PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(mk_tests
    tests/test_main.cpp
    tests/test_config.cpp
    tests/test_engine.cpp
    tests/test_hook.cpp
)
target_include_directories(mk_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(mk_tests PRIVATE mkcore winmm)
if(MSVC)
    target_compile_options(mk_tests PRIVATE /W4 /permissive- /utf-8 /wd5285)
endif()
add_test(NAME mk_tests COMMAND mk_tests)

add_executable(injecthold tools/injecthold.cpp)
if(MSVC)
    target_compile_options(injecthold PRIVATE /W4 /permissive- /utf-8)
endif()
target_compile_definitions(injecthold PRIVATE UNICODE _UNICODE)

add_executable(mousekey WIN32
    src/main.cpp
    src/app.cpp
    src/ui.cpp
    src/tray.cpp
    res/mousekey.rc
)
target_link_libraries(mousekey PRIVATE mkcore imgui d3d11 dxgi imm32 winmm)
target_include_directories(mousekey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/res)
if(MSVC)
    target_compile_options(mousekey PRIVATE /W4 /permissive- /utf-8)
endif()
target_compile_definitions(mousekey PRIVATE UNICODE _UNICODE)
target_link_options(mousekey PRIVATE "/MANIFEST:NO")
