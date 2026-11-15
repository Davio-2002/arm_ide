cmake_minimum_required(VERSION 3.24)
project(arm_ide CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fail early on compilers lacking the library parts the code uses.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS ${CMAKE_CXX23_STANDARD_COMPILE_OPTION})
check_cxx_source_compiles([[
    #include <algorithm>
    #include <span>
    #include <string>
    #include <vector>
    int main() {
        std::vector<int> v;
        std::ranges::stable_sort( v );
        std::span<int> s( v );
        return std::string( "ab" ).contains( 'a' ) && std::string( "ab" ).starts_with( "a" ) ? 0 : s.size();
    }
]] HAS_CXX23_LIBRARY)
unset(CMAKE_REQUIRED_FLAGS)
if(NOT HAS_CXX23_LIBRARY)
    message(FATAL_ERROR "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION} lacks the C++23 "
        "library the IDE needs (std::string::contains). Use a newer compiler, for example: "
        "rm -rf build && cmake --preset release -DCMAKE_CXX_COMPILER=g++")
endif()

# Commit archives instead of git clones: a clone fetches the whole history (~25 s, 161 MB).
include(FetchContent)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/cf53467ab0db7a783f04b32dd9160e6aa5b720ef.tar.gz
    URL_HASH SHA256=d4ee73f3f993f9ddf2fafd86c1b86ecba3eebe5146c432925118bc53cba5c63a)
FetchContent_Declare(text_editor
    URL https://github.com/goossens/ImGuiColorTextEdit/archive/0a489da863f22f7493049f5c6eb85355f405b02e.tar.gz
    URL_HASH SHA256=a8622bfbb205e48fed5b7b50701b255bb919523b798403d220ea51792a908a8d)
FetchContent_Declare(nanosvg
    URL https://github.com/memononen/nanosvg/archive/239e102ec2c691f2902e20ace2ed36ee4a35cfe6.tar.gz
    URL_HASH SHA256=2bc68bdb518d7800252042e5cad50a0ab321596f0cbf49ef2a752926329063d2
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(imgui text_editor nanosvg)

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
pkg_check_modules(VTERM REQUIRED IMPORTED_TARGET vterm)

# core <- app <- gui. The libraries don't link ImGui, so GUI includes there fail to build.
add_library(arm_ide_core STATIC
    src/core/process.cpp
    src/core/toolchain.cpp
    src/core/header_declarations.cpp
    src/core/gdb_remote.cpp
    src/core/debugger.cpp
    src/core/terminal.cpp)
target_include_directories(arm_ide_core PUBLIC src)
target_link_libraries(arm_ide_core PUBLIC PkgConfig::VTERM util)

add_library(arm_ide_app STATIC
    src/app/session.cpp
    src/app/documents.cpp
    src/app/completion.cpp
    src/app/aarch64_words.cpp)
target_link_libraries(arm_ide_app PUBLIC arm_ide_core)

add_executable(arm_ide
    src/gui/main.cpp
    src/gui/window.cpp
    src/gui/settings.cpp
    src/gui/dialogs.cpp
    src/gui/appearance.cpp
    src/gui/fonts.cpp
    src/gui/commands.cpp
    src/gui/menu.cpp
    src/gui/theme.cpp
    src/gui/color_schemes.cpp
    src/gui/icons.cpp
    src/gui/toolbar.cpp
    src/gui/widgets.cpp
    src/gui/statusbar.cpp
    src/gui/syntax.cpp
    src/gui/panels/panels.cpp
    src/gui/panels/editor.cpp
    src/gui/panels/console.cpp
    src/gui/panels/terminal_view.cpp
    src/gui/panels/registers.cpp
    src/gui/panels/memory.cpp)
target_include_directories(arm_ide PRIVATE ${nanosvg_SOURCE_DIR}/src)
target_link_libraries(arm_ide PRIVATE arm_ide_app imgui)

# Embedded files as byte lists for #include (#embed needs clang 19; Ubuntu 24.04 has 18).
function(embed_as_bytes input output)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${input})
    file(READ ${input} hex HEX)
    string(REGEX REPLACE "(..)" "0x\\1," bytes "${hex}")
    file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/generated/${output} CONTENT "${bytes}\n")
endfunction()

embed_as_bytes(${CMAKE_SOURCE_DIR}/assets/app_icon/icon.rgba icon_rgba.inc)
# Every icon becomes <name>_svg.inc, included by gui/icons.cpp.
file(GLOB icon_files CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/icons/*.svg)
foreach(icon_file ${icon_files})
    get_filename_component(icon ${icon_file} NAME_WE)
    embed_as_bytes(${icon_file} ${icon}_svg.inc)
endforeach()
target_include_directories(arm_ide PRIVATE ${CMAKE_BINARY_DIR}/generated)
