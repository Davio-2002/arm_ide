#include <cstdlib>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vterm.h>

#include <core/terminal.h>

static const size_t maxScrollbackLines = 10000;

static uint32_t packColor( const VTermColor &color ) {
    return ( color.rgb.red << 16 ) | ( color.rgb.green << 8 ) | color.rgb.blue;
}

static TerminalCell convertCell( const VTermScreen *screen, VTermScreenCell cell ) {
    TerminalCell result;
    // libvterm marks the right half of a wide character with 0xffffffff.
    result.character = cell.chars[0] == 0xffffffff ? 0 : cell.chars[0];
    result.width = cell.width;
    result.defaultForeground = VTERM_COLOR_IS_DEFAULT_FG( &cell.fg );
    result.defaultBackground = VTERM_COLOR_IS_DEFAULT_BG( &cell.bg );
    vterm_screen_convert_color_to_rgb( screen, &cell.fg );
    vterm_screen_convert_color_to_rgb( screen, &cell.bg );
    result.foreground = packColor( cell.fg );
    result.background = packColor( cell.bg );
    result.bold = cell.attrs.bold;
    result.underline = cell.attrs.underline;
    result.reverse = cell.attrs.reverse;
    return result;
}

static void writeToShell( const char *bytes, size_t length, void *user ) {
    Terminal &terminal = *static_cast<Terminal *>( user );
    while ( length > 0 && terminal.masterFd >= 0 ) {
        ssize_t written = write( terminal.masterFd, bytes, length );
        if ( written <= 0 ) {
            return;
        }
        bytes += written;
        length -= written;
    }
}

static int setProperty( VTermProp property, VTermValue *value, void *user ) {
    if ( property == VTERM_PROP_CURSORVISIBLE ) {
        static_cast<Terminal *>( user )->cursorVisible = value->boolean;
    }
    return 1;
}

static int pushScrollbackLine( int columns, const VTermScreenCell *cells, void *user ) {
    Terminal &terminal = *static_cast<Terminal *>( user );
    TerminalLine line;
    for ( int i = 0; i < columns; i++ ) {
        line.push_back( convertCell( terminal.screen, cells[i] ) );
    }
    terminal.scrollback.push_back( std::move( line ) );
    if ( terminal.scrollback.size() > maxScrollbackLines ) {
        terminal.scrollback.pop_front();
    }
    return 1;
}

static int clearScrollback( void *user ) {
    static_cast<Terminal *>( user )->scrollback.clear();
    return 1;
}

static VTermScreenCallbacks screenCallbacks = {
    .settermprop = setProperty,
    .sb_pushline = pushScrollbackLine,
    .sb_clear = clearScrollback,
};

static void freeScreen( Terminal &terminal ) {
    if ( terminal.vterm ) {
        vterm_free( terminal.vterm );
    }
    terminal.vterm = nullptr;
    terminal.screen = nullptr;
    terminal.scrollback.clear();
}

static void closeShell( Terminal &terminal ) {
    close( terminal.masterFd );
    terminal.masterFd = -1;
    terminal.pid = -1;
}

// The shell is the user's login shell, interactive because it runs on a terminal.
bool startTerminal( Terminal &terminal, const std::string &directory, int rows, int columns ) {
    freeScreen( terminal );
    terminal.exited = false;
    terminal.cursorVisible = true;
    terminal.rows = rows;
    terminal.columns = columns;

    winsize size = {};
    size.ws_row = rows;
    size.ws_col = columns;
    pid_t pid = forkpty( &terminal.masterFd, nullptr, nullptr, &size );
    if ( pid < 0 ) {
        terminal.masterFd = -1;
        return false;
    }
    if ( pid == 0 ) {
        if ( !directory.empty() && chdir( directory.c_str() ) != 0 ) {
            dprintf( STDERR_FILENO, "Cannot enter %s\n", directory.c_str() );
        }
        setenv( "TERM", "xterm-256color", 1 );
        setenv( "COLORTERM", "truecolor", 1 );
        const char *shell = std::getenv( "SHELL" );
        if ( !shell || !*shell ) {
            shell = "/bin/bash";
        }
        execl( shell, shell, nullptr );
        dprintf( STDERR_FILENO, "Cannot run %s\n", shell );
        _exit( 127 );
    }
    terminal.pid = pid;
    fcntl( terminal.masterFd, F_SETFL, O_NONBLOCK );

    terminal.vterm = vterm_new( rows, columns );
    vterm_set_utf8( terminal.vterm, 1 );
    vterm_output_set_callback( terminal.vterm, writeToShell, &terminal );
    terminal.screen = vterm_obtain_screen( terminal.vterm );
    vterm_screen_set_callbacks( terminal.screen, &screenCallbacks, &terminal );
    vterm_screen_enable_altscreen( terminal.screen, 1 );
    vterm_screen_reset( terminal.screen, 1 );
    return true;
}

bool updateTerminal( Terminal &terminal ) {
    if ( !isRunning( terminal ) ) {
        return false;
    }

    bool changed = false;
    char buffer[16384];
    ssize_t count;
    while ( ( count = read( terminal.masterFd, buffer, sizeof( buffer ) ) ) > 0 ) {
        vterm_input_write( terminal.vterm, buffer, count );
        changed = true;
    }
    if ( changed ) {
        vterm_screen_flush_damage( terminal.screen );
    }

    int status;
    if ( waitpid( terminal.pid, &status, WNOHANG ) == terminal.pid ) {
        terminal.exitCode = WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
        terminal.exited = true;
        closeShell( terminal );
        changed = true;
    }
    return changed;
}

void resizeTerminal( Terminal &terminal, int rows, int columns ) {
    terminal.rows = rows;
    terminal.columns = columns;
    if ( terminal.vterm ) {
        vterm_set_size( terminal.vterm, rows, columns );
    }
    if ( isRunning( terminal ) ) {
        winsize size = {};
        size.ws_row = rows;
        size.ws_col = columns;
        ioctl( terminal.masterFd, TIOCSWINSZ, &size );
    }
}

static VTermKey vtermKey( TerminalKey key ) {
    switch ( key ) {
    case TerminalKey::None:
        break;
    case TerminalKey::Enter:
        return VTERM_KEY_ENTER;
    case TerminalKey::Tab:
        return VTERM_KEY_TAB;
    case TerminalKey::Backspace:
        return VTERM_KEY_BACKSPACE;
    case TerminalKey::Escape:
        return VTERM_KEY_ESCAPE;
    case TerminalKey::Up:
        return VTERM_KEY_UP;
    case TerminalKey::Down:
        return VTERM_KEY_DOWN;
    case TerminalKey::Left:
        return VTERM_KEY_LEFT;
    case TerminalKey::Right:
        return VTERM_KEY_RIGHT;
    case TerminalKey::Insert:
        return VTERM_KEY_INS;
    case TerminalKey::Delete:
        return VTERM_KEY_DEL;
    case TerminalKey::Home:
        return VTERM_KEY_HOME;
    case TerminalKey::End:
        return VTERM_KEY_END;
    case TerminalKey::PageUp:
        return VTERM_KEY_PAGEUP;
    case TerminalKey::PageDown:
        return VTERM_KEY_PAGEDOWN;
    default:
        return static_cast<VTermKey>( VTERM_KEY_FUNCTION(
            static_cast<int>( key ) - static_cast<int>( TerminalKey::F1 ) + 1 ) );
    }
    return VTERM_KEY_NONE;
}

void sendTerminalInput( Terminal &terminal, const TerminalInput &input ) {
    if ( !isRunning( terminal ) ) {
        return;
    }
    int modifiers = VTERM_MOD_NONE;
    if ( input.shift ) {
        modifiers |= VTERM_MOD_SHIFT;
    }
    if ( input.ctrl ) {
        modifiers |= VTERM_MOD_CTRL;
    }
    if ( input.alt ) {
        modifiers |= VTERM_MOD_ALT;
    }
    VTermModifier modifier = static_cast<VTermModifier>( modifiers );
    if ( input.key != TerminalKey::None ) {
        vterm_keyboard_key( terminal.vterm, vtermKey( input.key ), modifier );
    } else {
        vterm_keyboard_unichar( terminal.vterm, input.character, modifier );
    }
}

void pasteToTerminal( Terminal &terminal, const std::string &text ) {
    if ( !isRunning( terminal ) ) {
        return;
    }
    vterm_keyboard_start_paste( terminal.vterm );
    writeToShell( text.data(), text.size(), &terminal );
    vterm_keyboard_end_paste( terminal.vterm );
}

// Closing the pty hangs up the programs running in it; the shell itself is killed.
void stopTerminal( Terminal &terminal ) {
    if ( !isRunning( terminal ) ) {
        return;
    }
    pid_t pid = terminal.pid;
    closeShell( terminal );
    kill( pid, SIGKILL );
    waitpid( pid, nullptr, 0 );
    freeScreen( terminal );
}

bool isRunning( const Terminal &terminal ) {
    return terminal.pid > 0;
}

// Rows below zero are scrollback lines, -1 being the newest.
TerminalCell terminalCell( const Terminal &terminal, int row, int column ) {
    if ( row < 0 ) {
        int index = static_cast<int>( terminal.scrollback.size() ) + row;
        if ( index < 0 || column >= static_cast<int>( terminal.scrollback[index].size() ) ) {
            return {};
        }
        return terminal.scrollback[index][column];
    }
    if ( !terminal.screen || row >= terminal.rows || column >= terminal.columns ) {
        return {};
    }
    VTermScreenCell cell = {};
    vterm_screen_get_cell( terminal.screen, VTermPos{ row, column }, &cell );
    return convertCell( terminal.screen, cell );
}

void terminalCursor( const Terminal &terminal, int &row, int &column ) {
    VTermPos position = { 0, 0 };
    if ( terminal.vterm ) {
        vterm_state_get_cursorpos( vterm_obtain_state( terminal.vterm ), &position );
    }
    row = position.row;
    column = position.col;
}
