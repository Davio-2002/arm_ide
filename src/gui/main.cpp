#include <GLFW/glfw3.h>

#include <string>

#include <app/session.h>
#include <gui/appearance.h>
#include <gui/commands.h>
#include <gui/dialogs.h>
#include <gui/icons.h>
#include <gui/menu.h>
#include <gui/panels/panels.h>
#include <gui/statusbar.h>
#include <gui/window.h>

namespace {

/// Fixed address: requests and dialog callbacks hold it.
Session session;

bool quitRequested = false;

/// @brief Quits on window close once unsaved changes are discarded.
void handleCloseRequest( GLFWwindow *window ) {
    if ( !glfwWindowShouldClose( window ) ) {
        return;
    }
    glfwSetWindowShouldClose( window, GLFW_FALSE );
    if ( !isDialogOpen() ) {
        whenChangesDiscarded( session, [] { quitRequested = true; } );
    }
}

/// @return File path, "*" if unsaved, and the app name.
std::string windowTitle() {
    const Document &document = session.activeDocument();
    std::string const name = document.path.empty() ? "untitled" : document.path;
    return name + ( isModified( document ) ? " *" : "" ) + " - ARM IDE";
}

/// @brief Opens the file from the command line, else the most recent one.
void openStartupFile( int argc, char **argv ) {
    if ( argc > 1 ) {
        session.openFile( argv[1] );
    } else if ( !session.recentFiles.empty() ) {
        session.openFile( session.recentFiles.front() );
    }
}

} // anonymous namespace

/// Each frame: poll, draw (read-only, requesting changes), then run requests and dialog answers.
int main( int argc, char **argv ) {
    GLFWwindow *window = createWindow();
    if ( !window ) {
        return 1;
    }
    setupImGui( window );
    openStartupFile( argc, argv );

    while ( !quitRequested ) {
        waitForNextFrame( session.isProgramBusy() || isTerminalBusy() );
        session.update();
        updateTerminalView();
        std::string const droppedFile = takeDroppedFile();
        if ( !droppedFile.empty() ) {
            requestAction( [droppedFile]( Session &session ) { session.openFile( droppedFile ); } );
        }

        updateIcons();
        beginFrame();
        drawMenuBar( session );
        drawStatusBar( session );
        drawPanels( session );
        runRequests( session );
        drawDialogs();
        drawAppearance();
        endFrame( window );
        setWindowTitle( window, windowTitle() );
        handleCloseRequest( window );
    }

    session.stop();
    stopTerminalShell();
    shutdownWindow( window );
}
