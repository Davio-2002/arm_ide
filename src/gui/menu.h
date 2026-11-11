#pragma once

#include <functional>
#include <string>

class Session;

// Commands asked for this frame by a shortcut, a menu item or a toolbar button.
struct MenuActions {
    bool open = false;
    bool save = false;
    bool saveAs = false;
    std::string recentFile;
    bool zoomIn = false;
    bool zoomOut = false;
    bool resetZoom = false;
    bool build = false;
    bool run = false;
    bool stop = false;
    bool debugContinue = false;
    bool debugStep = false;
    bool toggleBreakpoint = false;
};

// Draws the menu bar and toolbar and returns what they and the shortcuts asked for.
MenuActions drawMenuBar( const Session &session );
void runMenuActions( Session &session, const MenuActions &actions );

void openDocument( Session &session, const std::string &path );
void whenChangesDiscarded( const Session &session, std::function<void()> action );
void openDocumentIfAllowed( Session &session, const std::string &path );
