#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <chrono>
#include <initializer_list>
#include <utility>
#include <vector>

#include <gui/fonts.h>
#include <gui/settings.h>
#include <gui/theme.h>
#include <gui/window.h>

namespace {

std::string droppedFilePath;

/// After input every frame is drawn for a while: ImGui answers some input a frame late, and
/// the autocomplete popup and tooltips appear after a delay.
const auto inputSettleTime = std::chrono::milliseconds( 500 );

/// Time between idle frames: the caret blink (0.8 s on, 0.4 s off) and shell output.
const auto idleWait = std::chrono::milliseconds( 400 );

std::chrono::steady_clock::time_point lastInput;

/// Set by window events that need a frame but are not ImGui input: resize, redraw, close, drop.
bool windowEventPending = false;

/// @brief Sets the embedded window icon in 32, 64, 128 and 256 pixel sizes.
///
/// Not used on Wayland: it takes the icon from the .desktop file matching the app id.
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

/// @brief GLFW scroll callback: Ctrl+wheel zooms the code, other scrolling goes to ImGui.
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

/// @brief GLFW focus callback: passes the event to ImGui and, when the window gets the focus,
///        reads the desktop theme again, since it may have been changed in another window.
/// @param[in] focused GLFW_TRUE if the window got the focus.
void focusCallback( GLFWwindow *window, int focused ) {
    ImGui_ImplGlfw_WindowFocusCallback( window, focused );
    if ( focused ) {
        followDesktopTheme();
    }
}

/// @brief GLFW drop callback: keeps the first dropped file for takeDroppedFile().
/// @param[in] count Number of dropped paths.
/// @param[in] paths Dropped paths.
void dropCallback( GLFWwindow *, int count, const char **paths ) {
    if ( count > 0 ) {
        droppedFilePath = paths[0];
        windowEventPending = true;
    }
}

void windowEventCallback( GLFWwindow * ) {
    windowEventPending = true;
}

void framebufferSizeCallback( GLFWwindow *, int, int ) {
    windowEventPending = true;
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

std::string takeDroppedFile() {
    return std::exchange( droppedFilePath, "" );
}

void setupImGui( GLFWwindow *window ) {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyTheme();
    loadSettings();
    followDesktopTheme();
    loadFonts();

    ImGui_ImplGlfw_InitForOpenGL( window, true );
    ImGui_ImplOpenGL3_Init( "#version 330" );
    glfwSetScrollCallback( window, scrollCallback );
    glfwSetWindowFocusCallback( window, focusCallback );
    glfwSetDropCallback( window, dropCallback );
    glfwSetWindowRefreshCallback( window, windowEventCallback );
    glfwSetWindowCloseCallback( window, windowEventCallback );
    glfwSetFramebufferSizeCallback( window, framebufferSizeCallback );
}

void shutdownWindow( GLFWwindow *window ) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow( window );
    glfwTerminate();
}

/// Under Wayland the window's own protocol traffic (frame callbacks, buffer releases) ends a
/// GLFW wait at once, so an idle wait goes on until the deadline unless input or a window
/// event arrived.
void waitForNextFrame( bool busy ) {
    using Clock = std::chrono::steady_clock;
    const ImGuiContext &context = *ImGui::GetCurrentContext();
    Clock::time_point now = Clock::now();
    if ( busy || now - lastInput < inputSettleTime ) {
        glfwWaitEventsTimeout( 0.001 );
    } else {
        Clock::time_point deadline = now + idleWait;
        while ( now < deadline && context.InputEventsQueue.empty() && !windowEventPending ) {
            glfwWaitEventsTimeout( std::chrono::duration<double>( deadline - now ).count() );
            now = Clock::now();
        }
    }
    if ( !context.InputEventsQueue.empty() ) {
        lastInput = Clock::now();
    }
    windowEventPending = false;
}

void beginFrame() {
    applyRequestedFonts();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void endFrame( GLFWwindow *window ) {
    ImGui::Render();
    int width, height;
    glfwGetFramebufferSize( window, &width, &height );
    glViewport( 0, 0, width, height );
    ImVec4 ground = ImGui::ColorConvertU32ToFloat4( currentScheme().ground );
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
