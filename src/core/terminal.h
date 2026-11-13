#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <sys/types.h>
#include <vector>

struct VTerm;
struct VTermScreen;

/// @brief One character cell of the terminal screen.
struct TerminalCell {
    char32_t character = 0;
    int width = 1;
    uint32_t foreground = 0;
    uint32_t background = 0;
    bool defaultForeground = true;
    bool defaultBackground = true;
    bool bold = false;
    bool underline = false;
    bool reverse = false;
};

using TerminalLine = std::vector<TerminalCell>;

/// @brief Special key sent to the terminal.
enum class TerminalKey {
    None,
    Enter,
    Tab,
    Backspace,
    Escape,
    Up,
    Down,
    Left,
    Right,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12
};

/// @brief One key press or one typed character, with the modifiers held.
struct TerminalInput {
    TerminalKey key = TerminalKey::None;
    char32_t character = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

/// @brief A shell on a pseudo-terminal, with libvterm keeping its screen.
///
/// It must keep a fixed address: libvterm callbacks receive a pointer to it.
struct Terminal {
    pid_t pid = -1;
    int masterFd = -1;
    VTerm *vterm = nullptr;
    VTermScreen *screen = nullptr;
    int rows = 0;
    int columns = 0;
    bool cursorVisible = true;
    int exitCode = 0;
    bool exited = false;
    std::deque<TerminalLine> scrollback;
};

/// @brief Starts the user's login shell on a new pseudo-terminal.
/// @param[in,out] terminal  Terminal to start; any previous screen is discarded.
/// @param[in]     directory Working directory of the shell; empty for the current one.
/// @param[in]     rows      Screen height in cells.
/// @param[in]     columns   Screen width in cells.
/// @return True if the shell was started.
bool startTerminal( Terminal &terminal, const std::string &directory, int rows, int columns );

/// @brief Feeds what the shell printed into the screen and notices if the shell exited.
/// @param[in,out] terminal Terminal to update.
/// @return True if anything changed.
bool updateTerminal( Terminal &terminal );

/// @brief Resizes the screen and tells the shell the new size.
/// @param[in,out] terminal Terminal to resize.
/// @param[in]     rows     New height in cells.
/// @param[in]     columns  New width in cells.
void resizeTerminal( Terminal &terminal, int rows, int columns );

/// @brief Sends one key press or character to the shell, encoded as the terminal would.
/// @param[in,out] terminal Running terminal.
/// @param[in]     input    Key or character with modifiers.
void sendTerminalInput( Terminal &terminal, const TerminalInput &input );

/// @brief Sends text to the shell as a bracketed paste.
/// @param[in,out] terminal Running terminal.
/// @param[in]     text     Text to paste.
void pasteToTerminal( Terminal &terminal, const std::string &text );

/// @brief Kills the shell and discards its screen.
///
/// Closing the pty hangs up the programs running in it.
/// @param[in,out] terminal Terminal to stop.
void stopTerminal( Terminal &terminal );

/// @param[in] terminal Terminal to check.
/// @return True while the shell runs.
bool isRunning( const Terminal &terminal );

/// @brief Reads one cell of the screen or the scrollback.
/// @param[in] terminal Terminal to read.
/// @param[in] row      Screen row; negative for scrollback lines, -1 being the newest.
/// @param[in] column   Column.
/// @return The cell, or an empty cell outside the screen.
TerminalCell terminalCell( const Terminal &terminal, int row, int column );

/// @brief Reads the cursor position.
/// @param[in]  terminal Terminal to read.
/// @param[out] row      Cursor row.
/// @param[out] column   Cursor column.
void terminalCursor( const Terminal &terminal, int &row, int &column );
