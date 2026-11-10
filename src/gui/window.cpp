#include "gui/window.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <misc/freetype/imgui_freetype.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <vector>

#include "app/session.h"

static const float uiFontSize = 22.0f;
static const float defaultCodeFontSize = 18.0f;
static const float minCodeFontSize = 10.0f;
static const float maxCodeFontSize = 48.0f;

static ImFont *codeFont = nullptr;
static float codeFontSize = defaultCodeFontSize;
static std::string droppedFilePath;
static std::string settingsPath;
static Session *settingsSession = nullptr;

// Wayland takes the icon from the .desktop file matching the app id instead.
static void setWindowIcon( GLFWwindow *window ) {
    static constexpr unsigned char pixels[] = {
#embed "../../assets/icon_32.rgba"
    };
    constexpr int size = 32;
    constexpr int scales[] = { 1, 2, 4, 8 };
    std::vector<unsigned char> scaled[std::size( scales )];
    std::vector<GLFWimage> images;
    for ( size_t i = 0; i < std::size( scales ); ++i ) {
        int scale = scales[i];
        int scaledSize = size * scale;
        scaled[i].resize( scaledSize * scaledSize * 4 );
        for ( int y = 0; y < scaledSize; ++y ) {
            for ( int x = 0; x < scaledSize; ++x ) {
                std::copy_n( &pixels[( ( y / scale ) * size + x / scale ) * 4], 4,
                             &scaled[i][( y * scaledSize + x ) * 4] );
            }
        }
        images.push_back( { scaledSize, scaledSize, scaled[i].data() } );
    }
    glfwSetWindowIcon( window, static_cast<int>( images.size() ), images.data() );
}

GLFWwindow *createWindow() {
    if ( !glfwInit() ) {
        return nullptr;
    }

    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
    glfwWindowHintString( GLFW_WAYLAND_APP_ID, "arm_ide" );
    glfwWindowHintString( GLFW_X11_CLASS_NAME, "arm_ide" );
    glfwWindowHintString( GLFW_X11_INSTANCE_NAME, "arm_ide" );
    GLFWwindow *window = glfwCreateWindow( 1400, 900, "ARM IDE", nullptr, nullptr );
    if ( !window ) {
        return nullptr;
    }

    if ( glfwGetPlatform() != GLFW_PLATFORM_WAYLAND ) {
        setWindowIcon( window );
    }
    glfwMakeContextCurrent( window );
    glfwSwapInterval( 1 );
    return window;
}

static std::string findFirstExistingFile( const std::vector<std::string> &paths ) {
    for ( const std::string &path : paths ) {
        if ( std::filesystem::exists( path ) ) {
            return path;
        }
    }
    return "";
}

static void loadFonts() {
    std::string home = std::getenv( "HOME" ) ? std::getenv( "HOME" ) : "";
    std::string uiFontPath = findFirstExistingFile( {
        "/usr/share/fonts/truetype/ubuntu/UbuntuSans[wdth,wght].ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    } );
    std::string codeFontPath = findFirstExistingFile( {
        home + "/.local/share/fonts/JetBrainsMonoNerd/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    } );

    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    if ( !uiFontPath.empty() ) {
        io.Fonts->AddFontFromFileTTF( uiFontPath.c_str() );
    }
    if ( !codeFontPath.empty() ) {
        codeFont = io.Fonts->AddFontFromFileTTF( codeFontPath.c_str() );
    }
}

static void setCodeFontSize( float size ) {
    codeFontSize = std::clamp( size, minCodeFontSize, maxCodeFontSize );
    ImGui::MarkIniSettingsDirty();
}

void pushCodeFont() {
    ImGui::PushFont( codeFont, codeFontSize );
}

void zoomCode( float step ) {
    setCodeFontSize( codeFontSize + step );
}

void resetZoom() {
    setCodeFontSize( defaultCodeFontSize );
}

static void scrollCallback( GLFWwindow *window, double xOffset, double yOffset ) {
    bool ctrlDown = glfwGetKey( window, GLFW_KEY_LEFT_CONTROL ) == GLFW_PRESS ||
                    glfwGetKey( window, GLFW_KEY_RIGHT_CONTROL ) == GLFW_PRESS;
    if ( ctrlDown ) {
        zoomCode( yOffset );
        return;
    }
    ImGui_ImplGlfw_ScrollCallback( window, xOffset, yOffset );
}

static void dropCallback( GLFWwindow *, int count, const char **paths ) {
    if ( count > 0 ) {
        droppedFilePath = paths[0];
    }
}

std::string takeDroppedFile() {
    return std::exchange( droppedFilePath, "" );
}

static void *openSettingsEntry( ImGuiContext *, ImGuiSettingsHandler *, const char * ) {
    settingsSession->recentFiles.clear();
    return settingsSession;
}

static void readSettingsLine( ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line ) {
    std::string text = line;
    if ( text.starts_with( "Zoom=" ) ) {
        codeFontSize =
            std::clamp( std::strtof( line + 5, nullptr ), minCodeFontSize, maxCodeFontSize );
    } else if ( text.starts_with( "Recent=" ) ) {
        settingsSession->recentFiles.push_back( text.substr( 7 ) );
    }
}

static void writeSettings( ImGuiContext *, ImGuiSettingsHandler *handler,
                           ImGuiTextBuffer *buffer ) {
    buffer->appendf( "[%s][Settings]\n", handler->TypeName );
    buffer->appendf( "Zoom=%g\n", codeFontSize );
    for ( const std::string &path : settingsSession->recentFiles ) {
        buffer->appendf( "Recent=%s\n", path.c_str() );
    }
    buffer->append( "\n" );
}

static void loadSettings( Session &session ) {
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

static ImVec4 rgb( int red, int green, int blue, float alpha = 1.0f ) {
    return ImVec4( red / 255.0f, green / 255.0f, blue / 255.0f, alpha );
}

static void applyTheme() {
    ImGuiStyle &style = ImGui::GetStyle();
    style.FontSizeBase = uiFontSize;
    style.WindowPadding = ImVec2( 10.0f, 10.0f );
    style.FramePadding = ImVec2( 8.0f, 5.0f );
    style.ItemSpacing = ImVec2( 8.0f, 6.0f );
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.TabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBarBorderSize = 0.0f;
    style.DockingSeparatorSize = 1.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4 background = rgb( 29, 32, 33 );
    ImVec4 bar = rgb( 23, 25, 26 );
    ImVec4 surface = rgb( 50, 48, 47 );
    ImVec4 raised = rgb( 69, 64, 61 );
    ImVec4 border = rgb( 60, 56, 54 );
    ImVec4 accent = rgb( 254, 128, 25 );
    ImVec4 accentSoft = rgb( 254, 128, 25, 0.30f );
    ImVec4 accentHover = rgb( 254, 128, 25, 0.18f );
    ImVec4 transparent = rgb( 0, 0, 0, 0.0f );

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = rgb( 235, 219, 178 );
    colors[ImGuiCol_TextDisabled] = rgb( 146, 131, 116 );
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = background;
    colors[ImGuiCol_PopupBg] = surface;
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_FrameBg] = surface;
    colors[ImGuiCol_FrameBgHovered] = raised;
    colors[ImGuiCol_FrameBgActive] = raised;
    colors[ImGuiCol_TitleBg] = bar;
    colors[ImGuiCol_TitleBgActive] = bar;
    colors[ImGuiCol_TitleBgCollapsed] = bar;
    colors[ImGuiCol_MenuBarBg] = bar;
    colors[ImGuiCol_ScrollbarBg] = transparent;
    colors[ImGuiCol_ScrollbarGrab] = raised;
    colors[ImGuiCol_ScrollbarGrabHovered] = rgb( 102, 92, 84 );
    colors[ImGuiCol_ScrollbarGrabActive] = rgb( 124, 111, 100 );
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_Button] = surface;
    colors[ImGuiCol_ButtonHovered] = raised;
    colors[ImGuiCol_ButtonActive] = accentSoft;
    colors[ImGuiCol_Header] = accentSoft;
    colors[ImGuiCol_HeaderHovered] = accentHover;
    colors[ImGuiCol_HeaderActive] = accentSoft;
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_ResizeGrip] = transparent;
    colors[ImGuiCol_ResizeGripHovered] = accentSoft;
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_Tab] = bar;
    colors[ImGuiCol_TabHovered] = surface;
    colors[ImGuiCol_TabSelected] = background;
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabDimmed] = bar;
    colors[ImGuiCol_TabDimmedSelected] = background;
    colors[ImGuiCol_TabDimmedSelectedOverline] = transparent;
    colors[ImGuiCol_DockingPreview] = accentSoft;
    colors[ImGuiCol_DockingEmptyBg] = background;
    colors[ImGuiCol_TableRowBg] = transparent;
    colors[ImGuiCol_TableRowBgAlt] = rgb( 235, 219, 178, 0.03f );
    colors[ImGuiCol_TextSelectedBg] = accentSoft;
    colors[ImGuiCol_NavCursor] = accent;
}

void setupImGui( GLFWwindow *window, Session &session ) {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    loadFonts();
    applyTheme();
    loadSettings( session );

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
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void endFrame( GLFWwindow *window ) {
    ImGui::Render();
    int width, height;
    glfwGetFramebufferSize( window, &width, &height );
    glViewport( 0, 0, width, height );
    glClearColor( 0.08f, 0.08f, 0.10f, 1.0f );
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
