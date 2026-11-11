#pragma once

#include <functional>
#include <string>

class Session;

struct MenuActions {
    bool open = false;
    bool save = false;
    bool saveAs = false;
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
};

// "~/..." for paths under the home folder.
std::string displayPath( const std::string &path );

MenuActions drawMenuBar( const Session &session );
void runMenuActions( Session &session, const MenuActions &actions );

void openDocument( Session &session, const std::string &path );
void whenChangesDiscarded( std::function<void()> action );
