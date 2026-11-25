#pragma once

#include <imgui.h>

class Session;
struct TerminalView;

/// @brief Syncs the editor with the session, then draws the tool strip, dock space and
///        visible panels. New panels go in the `panels[]` table in panels.cpp.
void drawPanels( Session &session, TerminalView &terminalView );

/// @brief Begins a panel window drawn as a rounded rect with its own header; end with
///        ImGui::End.
/// @param[in] name Window name and dock ID.
void beginPanel( const char *name, ImGuiWindowFlags flags = 0 );

/// @brief Draws a title aligned with the header buttons.
void drawPanelTitle( const char *title );

/// @brief Makes icon buttons as tall as the header row; pop with ImGui::PopStyleVar.
void pushHeaderButtonPadding();

float headerButtonWidth();

// Panels (the Editor is in editor.h)

/// @brief Console, Build and Terminal tabs; switches to Build after a failed build, to
///        Console after a good one.
void drawConsolePanel( const Session &session, TerminalView &terminalView );

/// @brief Flags, conditions, x0-x30, sp and pc; changes since the last pause highlighted.
void drawRegistersPanel( const Session &session );

/// @brief Hex and ASCII dump at a typed address, shown while paused.
void drawMemoryPanel( Session &session );
