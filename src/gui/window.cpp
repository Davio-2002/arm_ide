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

#include <app/session.h>
#include <gui/theme.h>
#include <gui/window.h>

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

GLFWwindow *createWindow() {
    if ( !glfwInit() ) {
        return nullptr;
    }

    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
#ifdef GLFW_WAYLAND_APP_ID // GLFW 3.4+
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
    } else if ( text.starts_with( "Recent=" ) && std::filesystem::exists( text.substr( 7 ) ) ) {
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

void setupImGui( GLFWwindow *window, Session &session ) {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    loadFonts();
    applyIslandsTheme();
    ImGui::GetStyle().FontSizeBase = uiFontSize;
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
    ImVec4 ground = ImGui::ColorConvertU32ToFloat4( groundColor );
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
