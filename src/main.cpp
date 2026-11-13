#include <GLFW/glfw3.h>

#include <filesystem>
#include <string>

#include <app/session.h>
#include <gui/dialogs.h>
#include <gui/icons.h>
#include <gui/menu.h>
#include <gui/panels/editor.h>
#include <gui/panels/panels.h>
#include <gui/statusbar.h>
#include <gui/window.h>

static Session session;

static bool quitRequested = false;

/// @brief Turns a window close into a quit request, after the user agrees to discard unsaved
///        changes.
/// @param[in] window Main window.
static void handleCloseRequest( GLFWwindow *window ) {
    if ( !glfwWindowShouldClose( window ) ) {
        return;
    }
    glfwSetWindowShouldClose( window, GLFW_FALSE );
    if ( !isDialogOpen() ) {
        whenChangesDiscarded( [] { quitRequested = true; } );
    }
}

/// @return Window title: file path, "*" if unsaved, and the app name.
static std::string windowTitle() {
    std::string name = session.filePath.empty() ? "untitled" : session.filePath;
    return name + ( isEditorModified() ? " *" : "" ) + " - ARM IDE";
}

/// @brief Opens the file given on the command line, or else the most recent file.
/// @param[in] argc Argument count of main.
/// @param[in] argv Arguments of main.
static void openStartupFile( int argc, char **argv ) {
    if ( argc > 1 ) {
        openDocument( session, argv[1] );
    } else if ( !session.recentFiles.empty() &&
                std::filesystem::exists( session.recentFiles.front() ) ) {
        openDocument( session, session.recentFiles.front() );
    }
}

int main( int argc, char **argv ) {
    GLFWwindow *window = createWindow();
    if ( !window ) {
        return 1;
    }
    setupImGui( window, session );
    loadIcons();
    setupEditor( session );
    openStartupFile( argc, argv );

    while ( !quitRequested ) {
        bool busy = session.isProgramBusy() || session.isTerminalBusy();
        glfwWaitEventsTimeout( busy ? 0.001 : 0.1 );
        session.update();
        std::string droppedFile = takeDroppedFile();
        if ( !droppedFile.empty() && !isDialogOpen() ) {
            openDocument( session, droppedFile );
        }

        beginFrame();
        MenuActions actions = drawMenuBar( session );
        drawStatusBar( session );
        drawPanels( session, actions );
        runMenuActions( session, actions );
        drawDialogs();
        endFrame( window );
        setWindowTitle( window, windowTitle() );
        handleCloseRequest( window );
    }

    session.stop();
    session.stopTerminal();
    shutdownWindow( window );
}
