#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <misc/freetype/imgui_freetype.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include <app/session.h>
#include <core/fonts.h>
#include <gui/theme.h>
#include <gui/window.h>

namespace {

const float uiFontSize = 22.0f;
const float defaultCodeFontSize = 18.0f;
const float minCodeFontSize = 10.0f;
const float maxCodeFontSize = 48.0f;

float codeFontSize = defaultCodeFontSize;
std::string droppedFilePath;
std::string settingsPath;
Session *settingsSession = nullptr;

/// @brief Sets the embedded window icon in 32, 64, 128 and 256 pixel sizes.
///
/// Not used on Wayland: it takes the icon from the .desktop file matching the app id.
/// @param[in] window Main window.
void setWindowIcon( GLFWwindow *window ) {
    static unsigned char pixels[] = {
#include "icon_rgba.inc"
    };
    std::vector<GLFWimage> images;
    unsigned char *image = pixels;
    for ( int size : { 32, 64, 128, 256 } ) {
        images.push_back( { size, size, image } );
        image += size * size * 4;
    }
    glfwSetWindowIcon( window, static_cast<int>( images.size() ), images.data() );
}

} // anonymous namespace

GLFWwindow *createWindow() {
    if ( !glfwInit() ) {
        return nullptr;
    }

    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
#ifdef GLFW_WAYLAND_APP_ID
    glfwWindowHintString( GLFW_WAYLAND_APP_ID, "arm_ide" );
#endif
    glfwWindowHintString( GLFW_X11_CLASS_NAME, "arm_ide" );
    glfwWindowHintString( GLFW_X11_INSTANCE_NAME, "arm_ide" );
    GLFWwindow *window = glfwCreateWindow( 1400, 900, "ARM IDE", nullptr, nullptr );
    if ( !window ) {
        return nullptr;
    }

#ifdef GLFW_PLATFORM_WAYLAND
    if ( glfwGetPlatform() != GLFW_PLATFORM_WAYLAND ) {
        setWindowIcon( window );
    }
#else
    setWindowIcon( window );
#endif
    glfwMakeContextCurrent( window );
    glfwSwapInterval( 1 );
    return window;
}

namespace {

/// @brief A font in use, loaded from an installed family.
struct LoadedFont {
    std::string family;
    std::string requestedFamily;
    ImFont *font = nullptr;
};

LoadedFont uiFont;
LoadedFont codeFont;

/// @brief Loads the first of the families that is installed and readable, in place of the
///        font's current one. When none is, the font is kept, or ImGui's built-in font is used
///        if there is none yet.
/// @param[in,out] loaded     Font to replace; its family becomes the one loaded.
/// @param[in]     candidates Families in order of preference; empty names are skipped.
void loadFont( LoadedFont &loaded, const std::vector<std::string> &candidates ) {
    ImFontAtlas *atlas = ImGui::GetIO().Fonts;
    ImFont *font = nullptr;
    std::string family;
    for ( const std::string &candidate : candidates ) {
        std::optional<FontFace> face = findFont( candidate );
        if ( !face ) {
            continue;
        }
        ImFontConfig config;
        config.FontNo = face->index;
        config.Flags |= ImFontFlags_NoLoadError;
        font = atlas->AddFontFromFileTTF( face->path.c_str(), 0.0f, &config );
        if ( font ) {
            family = candidate;
            break;
        }
    }
    if ( !font && !loaded.font ) {
        font = atlas->AddFontDefault();
    }
    if ( !font ) {
        return;
    }
    if ( loaded.font ) {
        atlas->RemoveFont( loaded.font );
    }
    loaded.font = font;
    loaded.family = family;
}

/// @param[in] first       Family tried first.
/// @param[in] recommended Families tried after it.
/// @return The families in that order.
std::vector<std::string> fontCandidates( const std::string &first,
                                         std::initializer_list<const char *> recommended ) {
    std::vector<std::string> candidates = { first };
    candidates.insert( candidates.end(), recommended.begin(), recommended.end() );
    return candidates;
}

/// @brief Loads the interface and code fonts chosen in the settings, or else the first
///        recommended one installed.
void loadFonts() {
    ImGui::GetIO().Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    loadFont( uiFont, fontCandidates( uiFont.requestedFamily, { "Ubuntu Sans", "Inter", "Noto Sans",
                                                                "Cantarell", "DejaVu Sans" } ) );
    loadFont( codeFont, fontCandidates( codeFont.requestedFamily,
                                        { "JetBrains Mono", "JetBrainsMono Nerd Font", "Fira Code",
                                          "Cascadia Code", "Source Code Pro", "Ubuntu Mono",
                                          "DejaVu Sans Mono" } ) );
    uiFont.requestedFamily.clear();
    codeFont.requestedFamily.clear();
    ImGui::GetIO().FontDefault = uiFont.font;
}

/// @brief Loads a font family asked for with setUiFontFamily() or setCodeFontFamily().
///
/// Runs between frames: the old font can only be removed when no draw data uses it.
/// @param[in,out] loaded Font whose requested family is loaded; kept if it can't be.
void applyRequestedFont( LoadedFont &loaded ) {
    if ( loaded.requestedFamily.empty() ) {
        return;
    }
    loadFont( loaded, { loaded.requestedFamily } );
    loaded.requestedFamily.clear();
    ImGui::MarkIniSettingsDirty();
}

} // anonymous namespace

const std::string &uiFontFamily() {
    return uiFont.family;
}

const std::string &codeFontFamily() {
    return codeFont.family;
}

void setUiFontFamily( const std::string &family ) {
    uiFont.requestedFamily = family;
}

void setCodeFontFamily( const std::string &family ) {
    codeFont.requestedFamily = family;
}

namespace {

/// @brief Sets the code font size, clamped to its limits, and marks the settings for saving.
/// @param[in] size New size in points.
void setCodeFontSize( float size ) {
    codeFontSize = std::clamp( size, minCodeFontSize, maxCodeFontSize );
    ImGui::MarkIniSettingsDirty();
}

} // anonymous namespace

void pushCodeFont() {
    ImGui::PushFont( codeFont.font, codeFontSize );
}

void zoomCode( float step ) {
    setCodeFontSize( codeFontSize + step );
}

void resetZoom() {
    setCodeFontSize( defaultCodeFontSize );
}

namespace {

/// @brief GLFW scroll callback: Ctrl+wheel zooms the code, other scrolling goes to ImGui.
/// @param[in] window  Main window.
/// @param[in] xOffset Horizontal scroll.
/// @param[in] yOffset Vertical scroll.
void scrollCallback( GLFWwindow *window, double xOffset, double yOffset ) {
    bool ctrlDown = glfwGetKey( window, GLFW_KEY_LEFT_CONTROL ) == GLFW_PRESS ||
                    glfwGetKey( window, GLFW_KEY_RIGHT_CONTROL ) == GLFW_PRESS;
    if ( ctrlDown ) {
        zoomCode( yOffset );
        return;
    }
    ImGui_ImplGlfw_ScrollCallback( window, xOffset, yOffset );
}

/// @brief GLFW drop callback: keeps the first dropped file for takeDroppedFile().
/// @param[in] count Number of dropped paths.
/// @param[in] paths Dropped paths.
void dropCallback( GLFWwindow *, int count, const char **paths ) {
    if ( count > 0 ) {
        droppedFilePath = paths[0];
    }
}

} // anonymous namespace

std::string takeDroppedFile() {
    return std::exchange( droppedFilePath, "" );
}

namespace {

/// @brief ImGui settings callback: starts reading the IDE's section of the settings file.
/// @return The session, passed to readSettingsLine() as the entry.
void *openSettingsEntry( ImGuiContext *, ImGuiSettingsHandler *, const char * ) {
    settingsSession->recentFiles.clear();
    return settingsSession;
}

/// @brief ImGui settings callback: applies one line of the IDE's section: zoom, fonts, colours or
///        a recent file that still exists.
/// @param[in] line Line in "Key=value" form.
void readSettingsLine( ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line ) {
    std::string text = line;
    if ( text.starts_with( "Zoom=" ) ) {
        codeFontSize =
            std::clamp( std::strtof( line + 5, nullptr ), minCodeFontSize, maxCodeFontSize );
    } else if ( text.starts_with( "UiFont=" ) ) {
        uiFont.requestedFamily = text.substr( 7 );
    } else if ( text.starts_with( "CodeFont=" ) ) {
        codeFont.requestedFamily = text.substr( 9 );
    } else if ( text.starts_with( "Accent=" ) ) {
        setAccentColor( static_cast<ImU32>( std::strtoul( line + 7, nullptr, 16 ) ) );
    } else if ( text.starts_with( "PanelBackground=" ) ) {
        setIslandColor( static_cast<ImU32>( std::strtoul( line + 16, nullptr, 16 ) ) );
    } else if ( text.starts_with( "Recent=" ) && std::filesystem::exists( text.substr( 7 ) ) ) {
        settingsSession->recentFiles.push_back( text.substr( 7 ) );
    }
}

/// @brief ImGui settings callback: writes the IDE's section of the settings file.
/// @param[in]     handler The IDE's settings handler.
/// @param[in,out] buffer  Settings text being written.
void writeSettings( ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buffer ) {
    buffer->appendf( "[%s][Settings]\n", handler->TypeName );
    buffer->appendf( "Zoom=%g\n", codeFontSize );
    buffer->appendf( "UiFont=%s\n", uiFont.family.c_str() );
    buffer->appendf( "CodeFont=%s\n", codeFont.family.c_str() );
    buffer->appendf( "Accent=%08X\n", accentColor() );
    buffer->appendf( "PanelBackground=%08X\n", islandColor() );
    for ( const std::string &path : settingsSession->recentFiles ) {
        buffer->appendf( "Recent=%s\n", path.c_str() );
    }
    buffer->append( "\n" );
}

/// @brief Registers the IDE's settings handler and loads $XDG_CONFIG_HOME/arm_ide/imgui.ini.
/// @param[in,out] session Session whose recent files are read and written.
void loadSettings( Session &session ) {
    const char *configHome = std::getenv( "XDG_CONFIG_HOME" );
    const char *home = std::getenv( "HOME" );
    std::filesystem::path configDirectory =
        configHome ? configHome : std::string( home ? home : "." ) + "/.config";
    configDirectory /= "arm_ide";
    std::error_code ignored;
    std::filesystem::create_directories( configDirectory, ignored );
    settingsPath = configDirectory / "imgui.ini";

    settingsSession = &session;
    ImGuiSettingsHandler handler;
    handler.TypeName = "ArmIde";
    handler.TypeHash = ImHashStr( "ArmIde" );
    handler.ReadOpenFn = openSettingsEntry;
    handler.ReadLineFn = readSettingsLine;
    handler.WriteAllFn = writeSettings;
    ImGui::AddSettingsHandler( &handler );

    ImGui::GetIO().IniFilename = settingsPath.c_str();
    ImGui::LoadIniSettingsFromDisk( settingsPath.c_str() );
}

} // anonymous namespace

void setupImGui( GLFWwindow *window, Session &session ) {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyIslandsTheme();
    ImGui::GetStyle().FontSizeBase = uiFontSize;
    loadSettings( session );
    loadFonts();

    ImGui_ImplGlfw_InitForOpenGL( window, true );
    ImGui_ImplOpenGL3_Init( "#version 330" );
    glfwSetScrollCallback( window, scrollCallback );
    glfwSetDropCallback( window, dropCallback );
}

void shutdownWindow( GLFWwindow *window ) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow( window );
    glfwTerminate();
}

void beginFrame() {
    applyRequestedFont( uiFont );
    applyRequestedFont( codeFont );
    ImGui::GetIO().FontDefault = uiFont.font;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void endFrame( GLFWwindow *window ) {
    ImGui::Render();
    int width, height;
    glfwGetFramebufferSize( window, &width, &height );
    glViewport( 0, 0, width, height );
    ImVec4 ground = ImGui::ColorConvertU32ToFloat4( groundColor() );
    glClearColor( ground.x, ground.y, ground.z, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
    glfwSwapBuffers( window );
}

void setWindowTitle( GLFWwindow *window, const std::string &title ) {
    static std::string currentTitle;
    if ( title != currentTitle ) {
        currentTitle = title;
        glfwSetWindowTitle( window, title.c_str() );
    }
}
