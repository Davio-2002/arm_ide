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

    FetchContent_Declare(nanosvg
    GIT_REPOSITORY https://github.com/memononen/nanosvg.git
    GIT_TAG 239e102ec2c691f2902e20ace2ed36ee4a35cfe6
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

add_library(arm_ide_core STATIC
    src/core/process.cpp
    src/core/toolchain.cpp
    src/core/debugger.cpp
    src/core/terminal.cpp
    src/app/session.cpp)
target_include_directories(arm_ide_core PUBLIC src)
target_link_libraries(arm_ide_core PUBLIC PkgConfig::VTERM util)

add_executable(arm_ide
    src/main.cpp
    src/gui/window.cpp
    src/gui/dialogs.cpp
    src/gui/menu.cpp
    src/gui/theme.cpp
    src/gui/icons.cpp
    src/gui/toolbar.cpp
    src/gui/widgets.cpp
    src/gui/statusbar.cpp
    src/gui/panels/panels.cpp
    src/gui/panels/editor.cpp
    src/gui/panels/console.cpp
    src/gui/panels/terminal_view.cpp
    src/gui/panels/registers.cpp
    src/gui/panels/memory.cpp
    src/language/aarch64_language.cpp
    src/language/aarch64_words.cpp)
target_include_directories(arm_ide PRIVATE ${nanosvg_SOURCE_DIR}/src)
target_link_libraries(arm_ide PRIVATE arm_ide_core imgui)

# Files compiled in as C byte lists for #include (#embed needs clang 19+, Ubuntu 24.04 ships
# clang 18).
function(embed_as_bytes input output)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${input})
    file(READ ${input} hex HEX)
    string(REGEX REPLACE "(..)" "0x\\1," bytes "${hex}")
    file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/generated/${output} CONTENT "${bytes}\n")
endfunction()

embed_as_bytes(${CMAKE_SOURCE_DIR}/assets/app_icon/icon.rgba icon_rgba.inc)
foreach(icon build run debug stop resume stepOver asmFile console clear registers memory)
    embed_as_bytes(${CMAKE_SOURCE_DIR}/assets/icons/${icon}.svg ${icon}_svg.inc)
endforeach()
target_include_directories(arm_ide PRIVATE ${CMAKE_BINARY_DIR}/generated)
