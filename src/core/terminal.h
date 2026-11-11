#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <sys/types.h>
#include <vector>

struct VTerm;
struct VTermScreen;

struct TerminalCell {
    char32_t character = 0; // 0 for an empty cell and for the right half of a wide character
    int width = 1;
    uint32_t foreground = 0; // 0xRRGGBB
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

// One key press or one typed character, with the modifiers held.
struct TerminalInput {
    TerminalKey key = TerminalKey::None;
    char32_t character = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

// A shell on a pseudo-terminal, with libvterm keeping its screen. It must keep a fixed
// address: libvterm callbacks receive a pointer to it.
struct Terminal {
    pid_t pid = -1;
    int masterFd = -1;
    VTerm *vterm = nullptr;
    VTermScreen *screen = nullptr;
    int rows = 0;
    int columns = 0;
    bool cursorVisible = true;
    int exitCode = 0;
    bool exited = false; // the shell ended by itself; its last screen stays visible
    std::deque<TerminalLine> scrollback;
};

bool startTerminal( Terminal &terminal, const std::string &directory, int rows, int columns );
// Reads what the shell printed; returns true when anything arrived.
bool updateTerminal( Terminal &terminal );
void resizeTerminal( Terminal &terminal, int rows, int columns );
void sendTerminalInput( Terminal &terminal, const TerminalInput &input );
void pasteToTerminal( Terminal &terminal, const std::string &text );
void stopTerminal( Terminal &terminal );
bool isRunning( const Terminal &terminal );

TerminalCell terminalCell( const Terminal &terminal, int row, int column );
void terminalCursor( const Terminal &terminal, int &row, int &column );
