cmake_minimum_required(VERSION 3.20)

project(factor_manager LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MINGW)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(factor_third_party_headers INTERFACE)
target_include_directories(factor_third_party_headers SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(factor_manager
    src_cpp/main.cpp
    src_cpp/storage.cpp
    src_cpp/raygui_bridge.cpp
    src/cJSON.c
)

target_include_directories(factor_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app_include
)

target_link_libraries(factor_manager PRIVATE factor_third_party_headers)
target_link_libraries(factor_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libraylib.a
    opengl32
    gdi32
    winmm
    shell32
    user32
    kernel32
)

if (WIN32)
    target_compile_definitions(factor_manager PRIVATE PLATFORM_DESKTOP)
    add_custom_command(TARGET factor_manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/raylib.dll
            $<TARGET_FILE_DIR:factor_manager>/raylib.dll
    )
endif()
