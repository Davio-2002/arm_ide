#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <sys/types.h>
#include <vector>

struct VTerm;
struct VTermScreen;

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

struct TerminalInput {
    TerminalKey key = TerminalKey::None;
    char32_t character = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

/// @brief A shell on a pty with its screen kept by libvterm. Needs a fixed address:
///        libvterm callbacks get a pointer to it.
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

/// @brief Starts the user's login shell; a previous screen is discarded.
/// @param[in] directory Working directory; empty for the current one.
/// @param[in] rows      Height in cells.
/// @param[in] columns   Width in cells.
/// @return True if started.
bool startTerminal( Terminal &terminal, const std::string &directory, int rows, int columns );

/// @brief Feeds shell output into the screen and notices when the shell exits.
/// @return True if anything changed.
bool updateTerminal( Terminal &terminal );

/// @brief Resizes the screen and tells the shell.
void resizeTerminal( Terminal &terminal, int rows, int columns );

/// @brief Sends a key or character, encoded as a terminal would.
void sendTerminalInput( Terminal &terminal, const TerminalInput &input );

/// @brief Sends text as a bracketed paste.
void pasteToTerminal( Terminal &terminal, const std::string &text );

/// @brief Kills the shell; closing the pty hangs up programs running in it.
void stopTerminal( Terminal &terminal );

/// @return True while the shell runs.
bool isRunning( const Terminal &terminal );

/// @param[in] row Screen row; negative for scrollback, -1 the newest.
/// @return The cell, or an empty one outside the screen.
TerminalCell terminalCell( const Terminal &terminal, int row, int column );

void terminalCursor( const Terminal &terminal, int &row, int &column );
