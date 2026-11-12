#pragma once

#include <functional>
#include <string>
#include <vector>

#include <core/terminal.h>

class Session;

struct MenuActions {
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
    int terminalRows = 0; // set while the Terminal tab is shown
    int terminalColumns = 0;
    std::vector<TerminalInput> terminalInputs;
    std::string terminalPaste;
    bool stopTerminal = false;
    bool clearTerminal = false;
};

// "~/..." for paths under the home folder.
std::string displayPath( const std::string &path );

MenuActions drawMenuBar( const Session &session );
void runMenuActions( Session &session, const MenuActions &actions );

void openDocument( Session &session, const std::string &path );
void whenChangesDiscarded( std::function<void()> action );
