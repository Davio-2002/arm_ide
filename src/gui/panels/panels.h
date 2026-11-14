#pragma once

#include <imgui.h>

class Session;

/// @brief Syncs the editor with the session, then draws the tool strip, dock space and
///        visible panels. New panels go in the `panels[]` table in panels.cpp.
void drawPanels( Session &session );

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
void drawConsolePanel( const Session &session );

/// @brief Flags, conditions, x0-x30, sp and pc; changes since the last pause highlighted.
void drawRegistersPanel( const Session &session );

/// @brief Hex and ASCII dump at a typed address, shown while paused.
void drawMemoryPanel( Session &session );

// Terminal tab of the Console panel

/// @brief Draws the shell's screen and fits the terminal to it. The shell starts in the
///        active document's folder, and again on a key press after it exited.
void drawTerminalView( const Session &session );

/// @brief Gives the keyboard to the terminal.
void focusTerminalView();

/// @brief Feeds shell output into the screen; called once per frame.
void updateTerminalView();

/// @return True within 500 ms of terminal input or output, to keep frames fast.
bool isTerminalBusy();

bool isTerminalRunning();

/// @brief Kills the shell and its programs; the view starts a new one.
void stopTerminalShell();

/// @brief Clears the scrollback and sends Ctrl+L.
void clearTerminalView();
