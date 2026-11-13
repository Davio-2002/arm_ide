#pragma once

#include <imgui.h>

class Session;
struct MenuActions;

/// @brief Draws the tool strip, the dock space and all visible panels.
/// @param[in,out] session Current session.
/// @param[in,out] actions Receives the actions of the panels.
void drawPanels( Session &session, MenuActions &actions );

/// @brief Begins a panel window. End it with ImGui::End.
///
/// A docked panel is an island with its own header row instead of a dock tab.
/// @param[in] name  Window name, also its dock ID.
/// @param[in] flags ImGui window flags.
void beginPanel( const char *name, ImGuiWindowFlags flags = 0 );

/// @brief Draws a panel title aligned with the header buttons.
/// @param[in] title Title text.
void drawPanelTitle( const char *title );

/// @brief Pushes padding that makes icon buttons as tall as the header row. Undo with
///        ImGui::PopStyleVar.
void pushHeaderButtonPadding();

/// @return Width of a header icon button.
float headerButtonWidth();
