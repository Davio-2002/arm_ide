#pragma once

#include <functional>
#include <string>
#include <vector>

#include <core/terminal.h>

class Session;

/// @brief User actions collected while drawing one frame, run afterwards by runMenuActions().
///
/// Menus, shortcuts, the toolbar and the panels all fill the same struct, so every action
/// takes one path into the Session.
struct MenuActions {
    bool newFile = false;
    bool open = false;
    bool save = false;
    bool saveAs = false;
    bool openAccentColor = false;
    bool openPanelColor = false;
    std::string fileToOpen;
    bool zoomIn = false;
    bool zoomOut = false;
    bool resetZoom = false;
    bool build = false;
    bool run = false;
    bool stop = false;
    bool debugContinue = false;
    bool debugStep = false;
    bool toggleBreakpoint = false;
    bool clearOutput = false;
    bool sendProgramInput = false;
    std::string programInput;
    bool endProgramInput = false;
    int terminalRows = 0;
    int terminalColumns = 0;
    std::vector<TerminalInput> terminalInputs;
    std::string terminalPaste;
    bool stopTerminal = false;
    bool clearTerminal = false;
};

/// @brief Shortens a path for display.
/// @param[in] path Absolute path.
/// @return "~/..." for paths under the home folder, the path itself otherwise.
std::string displayPath( const std::string &path );

/// @brief Draws the main menu bar with the toolbar and reads the global shortcuts.
/// @param[in] session Current session.
/// @return Actions chosen in the menus or by shortcut.
MenuActions drawMenuBar( const Session &session );

/// @brief Runs the actions collected this frame. Nothing runs while a dialog is open.
///
/// An unsaved new file is saved first when the action needs a path, such as Build.
/// @param[in,out] session Session to act on.
/// @param[in]     actions Actions to run.
void runMenuActions( Session &session, const MenuActions &actions );

/// @brief Reads a file and opens it in an editor tab.
/// @param[in,out] session Session that reads the file.
/// @param[in]     path    File to open.
void openDocument( Session &session, const std::string &path );

/// @brief Runs an action at once, or after the user agrees to discard unsaved changes.
/// @param[in] action Action to run.
void whenChangesDiscarded( std::function<void()> action );
