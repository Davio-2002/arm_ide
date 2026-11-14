#pragma once

#include <imgui.h>

class Session;

/// @brief Brings the editor in line with the session, then draws the tool strip, the dock
///        space and every visible panel.
///
/// The panels are listed in one table in panels.cpp: a new panel is a file with its draw
/// function, declared below, and a row in that table.
/// @param[in,out] session Current session; drawing only reads it, the editor sync reports
///                        edit versions to it.
void drawPanels( Session &session );

/// @brief Begins a panel window. End it with ImGui::End.
///
/// A docked panel is a rounded rectangle with its own header row instead of a dock tab.
/// @param[in] name  Window name, also its dock ID.
/// @param[in] flags ImGui window flags.
void beginPanel( const char *name, ImGuiWindowFlags flags = 0 );

/// @brief Draws a panel title aligned with the header buttons.
void drawPanelTitle( const char *title );

/// @brief Pushes padding that makes icon buttons as tall as the header row. Undo with
///        ImGui::PopStyleVar.
void pushHeaderButtonPadding();

/// @return Width of a header icon button.
float headerButtonWidth();

// The panels; the Editor panel is in editor.h --------------------------------------------------

/// @brief Draws the Console panel with three tabs: program output, build log and terminal.
///
/// After each build the panel switches to the build log if it failed and to the program
/// output if it succeeded.
void drawConsolePanel( const Session &session );

/// @brief Draws the Registers panel: flags, the conditions that hold, and x0-x30, sp and pc.
///        Values changed since the previous pause are highlighted.
void drawRegistersPanel( const Session &session );

/// @brief Draws the Memory panel: a hex and ASCII dump at an address typed by the user.
///        Memory is shown only while the debugger is paused.
/// @param[in,out] session Session that reads the memory over the debugger.
void drawMemoryPanel( Session &session );

// The terminal, shown in a tab of the Console panel ------------------------------------------

/// @brief Draws the shell's screen in the rest of the panel and sizes the terminal to fit. The
///        shell starts when the view is first drawn, in the active document's folder.
///
/// It takes the keyboard after a click. The mouse wheel scrolls back, a drag selects text. A
/// key press after the shell exited starts a new one.
/// @param[in] session Current session, for the starting folder.
void drawTerminalView( const Session &session );

/// @brief Gives the keyboard to the terminal view.
void focusTerminalView();

/// @brief Feeds the shell's output into its screen. Called once per frame.
void updateTerminalView();

/// @return True within 500 ms of terminal input or output, so the frame loop stays fast
///         while the shell is in use.
bool isTerminalBusy();

/// @return True while the shell runs.
bool isTerminalRunning();

/// @brief Kills the shell and the programs in it; the view starts a new one.
void stopTerminalShell();

/// @brief Clears the scrollback and sends Ctrl+L to the shell.
void clearTerminalView();
