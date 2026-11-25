#include <GLFW/glfw3.h>

#include <string>

#include <app/session.h>
#include <gui/appearance.h>
#include <gui/commands.h>
#include <gui/dialogs.h>
#include <gui/icons.h>
#include <gui/menu.h>
#include <gui/panels/panels.h>
#include <gui/panels/terminal_view.h>
#include <gui/statusbar.h>
#include <gui/window.h>

namespace {

/// @brief Sets quitRequested on window close once unsaved changes are discarded.
/// @param[in] quitRequested Must outlive the dialog it may open.
void handleCloseRequest( GLFWwindow *window, Session &session, bool &quitRequested ) {
    if ( !glfwWindowShouldClose( window ) ) {
        return;
    }
    glfwSetWindowShouldClose( window, GLFW_FALSE );
    if ( !isDialogOpen() ) {
        whenChangesDiscarded( session, [&quitRequested] { quitRequested = true; } );
    }
}

/// @return File path, "*" if unsaved, and the app name.
std::string windowTitle( const Session &session ) {
    const Document &document = session.activeDocument();
    std::string const name = document.path.empty() ? "untitled" : document.path;
    return name + ( isModified( document ) ? " *" : "" ) + " - ARM IDE";
}

/// @brief Opens the file from the command line, else the most recent one.
void openStartupFile( Session &session, int argc, char **argv ) {
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
    // Fixed addresses: requests, dialog callbacks and libvterm hold them until the loop ends.
    Session session;
    TerminalView terminalView;
    bool quitRequested = false;
    openStartupFile( session, argc, argv );

    while ( !quitRequested ) {
        waitForNextFrame( session.isProgramBusy() || isTerminalBusy( terminalView ) );
        session.update();
        updateTerminalView( terminalView );
        std::string const droppedFile = takeDroppedFile();
        if ( !droppedFile.empty() ) {
            requestAction( [droppedFile]( Session &session ) { session.openFile( droppedFile ); } );
        }

        updateIcons();
        beginFrame();
        drawMenuBar( session );
        drawStatusBar( session );
        drawPanels( session, terminalView );
        runRequests( session );
        drawDialogs();
        drawAppearance();
        endFrame( window );
        setWindowTitle( window, windowTitle( session ) );
        handleCloseRequest( window, session, quitRequested );
    }

    session.stop();
    stopTerminalShell( terminalView );
    shutdownWindow( window );
}
