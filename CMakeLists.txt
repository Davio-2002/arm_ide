cmake_minimum_required(VERSION 3.24)
project(arm_ide CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG cf53467ab0db7a783f04b32dd9160e6aa5b720ef)
FetchContent_Declare(text_editor
    GIT_REPOSITORY https://github.com/goossens/ImGuiColorTextEdit.git
    GIT_TAG 0a489da863f22f7493049f5c6eb85355f405b02e)
FetchContent_MakeAvailable(imgui text_editor)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
    ${text_editor_SOURCE_DIR}/TextEditor.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${text_editor_SOURCE_DIR})
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL Freetype::Freetype)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UNICORN REQUIRED IMPORTED_TARGET unicorn)

add_library(arm_ide_core STATIC
    src/core/process.cpp
    src/core/toolchain.cpp
    src/core/emulator.cpp
    src/app/session.cpp)
target_include_directories(arm_ide_core PUBLIC src)
target_link_libraries(arm_ide_core PUBLIC PkgConfig::UNICORN)

add_executable(arm_ide
    src/main.cpp
    src/gui/window.cpp
    src/gui/dialogs.cpp
    src/gui/menu.cpp
    src/gui/panels.cpp
    src/gui/aarch64_language.cpp
    src/gui/aarch64_words.cpp)
target_link_libraries(arm_ide PRIVATE arm_ide_core imgui)

# Window icon pixels as a C byte list (#embed needs clang 19+, Ubuntu 24.04 ships clang 18).
set(ICON_RGBA ${CMAKE_SOURCE_DIR}/assets/icon.rgba)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ICON_RGBA})
file(READ ${ICON_RGBA} iconHex HEX)
string(REGEX REPLACE "(..)" "0x\\1," iconBytes "${iconHex}")
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/generated/icon_rgba.inc CONTENT "${iconBytes}\n")
target_include_directories(arm_ide PRIVATE ${CMAKE_BINARY_DIR}/generated)
