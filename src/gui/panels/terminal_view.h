#pragma once

#include <chrono>
#include <compare>
#include <cstddef>

#include <core/terminal.h>

class Session;

/// @brief A place between cells.
struct CellPosition {
    int line = 0;
    int column = 0;
    auto operator<=>( const CellPosition & ) const = default;
};

/// @brief The Terminal tab: a shell and what the view shows of it. Needs a fixed address,
///        like the Terminal inside it.
struct TerminalView {
    Terminal terminal;
    std::chrono::steady_clock::time_point lastActivity;
    bool focused = false;
    int scrollOffset = 0;
    bool selecting = false;
    bool hasSelection = false;
    CellPosition selectionAnchor;
    CellPosition selectionEnd;
    size_t selectionScrollbackSize = 0;

    TerminalView() = default;
    TerminalView( const TerminalView & ) = delete;
    TerminalView &operator=( const TerminalView & ) = delete;
};

/// @brief Draws the shell's screen and fits the terminal to it. The shell starts in the
///        active document's folder, and again on a key press after it exited.
void drawTerminalView( TerminalView &view, const Session &session );

/// @brief Gives the keyboard to the terminal.
void focusTerminalView( TerminalView &view );

/// @brief Feeds shell output into the screen; called once per frame.
void updateTerminalView( TerminalView &view );

/// @return True within 500 ms of terminal input or output, to keep frames fast.
bool isTerminalBusy( const TerminalView &view );

bool isTerminalRunning( const TerminalView &view );

/// @brief Kills the shell and its programs; the view starts a new one.
void stopTerminalShell( TerminalView &view );

/// @brief Clears the scrollback and sends Ctrl+L.
void clearTerminalView( TerminalView &view );
