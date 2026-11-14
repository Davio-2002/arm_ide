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

/// The one session. It keeps a fixed address: requests and dialog callbacks hold it.
Session session;

bool quitRequested = false;

/// @brief Turns a window close into a quit request, after the user agrees to discard unsaved
///        changes.
void handleCloseRequest( GLFWwindow *window ) {
    if ( !glfwWindowShouldClose( window ) ) {
        return;
    }
    glfwSetWindowShouldClose( window, GLFW_FALSE );
    if ( !isDialogOpen() ) {
        whenChangesDiscarded( session, [] { quitRequested = true; } );
    }
}

/// @return Window title: file path, "*" if unsaved, and the app name.
std::string windowTitle() {
    const Document &document = session.activeDocument();
    std::string name = document.path.empty() ? "untitled" : document.path;
    return name + ( isModified( document ) ? " *" : "" ) + " - ARM IDE";
}

/// @brief Opens the file given on the command line, or else the most recent file.
/// @param[in] argc Argument count of main.
/// @param[in] argv Arguments of main.
void openStartupFile( int argc, char **argv ) {
    if ( argc > 1 ) {
        session.openFile( argv[1] );
    } else if ( !session.recentFiles.empty() ) {
        session.openFile( session.recentFiles.front() );
    }
}

} // anonymous namespace

/// Each frame: collect program output, draw everything (drawing only reads the session and
/// requests changes), then run the requests and the dialogs' answers.
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
        std::string droppedFile = takeDroppedFile();
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
