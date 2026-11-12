#pragma once

#include <imgui.h>

class Session;
struct MenuActions;

void drawPanels( Session &session, MenuActions &actions );

// For the panels: a docked panel is an island with its own header row instead of a dock tab.
void beginPanel( const char *name, ImGuiWindowFlags flags = 0 );
void drawPanelTitle( const char *title );
// Header buttons as tall as the header row.
void pushHeaderButtonPadding();
float headerButtonWidth();
