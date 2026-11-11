#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>

#include <app/session.h>
#include <gui/menu.h>
#include <gui/terminal_view.h>
#include <gui/theme.h>
#include <gui/window.h>

static const int wheelLines = 3;

static bool focused = false;
static int scrollOffset = 0; // how many lines the view is scrolled back

// A place between cells: line is a screen row, negative for scrollback lines.
struct CellPosition {
    int line = 0;
    int column = 0;
    auto operator<=>( const CellPosition & ) const = default;
};

// The selection runs from anchor to end, which may come first. Its lines move up as new
// output pushes lines into the scrollback.
static bool selecting = false;
static bool hasSelection = false;
static CellPosition selectionAnchor;
static CellPosition selectionEnd;
static size_t selectionScrollbackSize = 0;

struct KeyMapping {
    ImGuiKey key;
    TerminalKey terminalKey;
};

static const KeyMapping specialKeys[] = {
    { ImGuiKey_Enter, TerminalKey::Enter },
    { ImGuiKey_KeypadEnter, TerminalKey::Enter },
    { ImGuiKey_Tab, TerminalKey::Tab },
    { ImGuiKey_Backspace, TerminalKey::Backspace },
    { ImGuiKey_Escape, TerminalKey::Escape },
    { ImGuiKey_UpArrow, TerminalKey::Up },
    { ImGuiKey_DownArrow, TerminalKey::Down },
    { ImGuiKey_LeftArrow, TerminalKey::Left },
    { ImGuiKey_RightArrow, TerminalKey::Right },
    { ImGuiKey_Insert, TerminalKey::Insert },
    { ImGuiKey_Delete, TerminalKey::Delete },
    { ImGuiKey_Home, TerminalKey::Home },
    { ImGuiKey_End, TerminalKey::End },
    { ImGuiKey_PageUp, TerminalKey::PageUp },
    { ImGuiKey_PageDown, TerminalKey::PageDown },
    { ImGuiKey_F1, TerminalKey::F1 },
    { ImGuiKey_F2, TerminalKey::F2 },
    { ImGuiKey_F3, TerminalKey::F3 },
    { ImGuiKey_F4, TerminalKey::F4 },
    { ImGuiKey_F5, TerminalKey::F5 },
    { ImGuiKey_F6, TerminalKey::F6 },
    { ImGuiKey_F7, TerminalKey::F7 },
    { ImGuiKey_F8, TerminalKey::F8 },
    { ImGuiKey_F9, TerminalKey::F9 },
    { ImGuiKey_F10, TerminalKey::F10 },
    { ImGuiKey_F11, TerminalKey::F11 },
    { ImGuiKey_F12, TerminalKey::F12 },
};

static void pasteClipboard( MenuActions &actions ) {
    const char *clipboard = ImGui::GetClipboardText();
    actions.terminalPaste += clipboard ? clipboard : "";
}

void focusTerminalView() {
    focused = true;
}

static TerminalInput makeInput( TerminalKey key, char32_t character ) {
    const ImGuiIO &io = ImGui::GetIO();
    TerminalInput input;
    input.key = key;
    input.character = character;
    input.shift = io.KeyShift;
    input.ctrl = io.KeyCtrl;
    input.alt = io.KeyAlt;
    return input;
}

// Takes the keys from ImGui navigation so Tab, arrows and Escape reach the shell. Ctrl+letter
// comes as a key, other text as typed characters; Ctrl+Shift+V pastes as in VS Code.
static void readKeyboard( ImGuiID owner, MenuActions &actions ) {
    const ImGuiIO &io = ImGui::GetIO();
    for ( const KeyMapping &mapping : specialKeys ) {
        ImGui::SetKeyOwner( mapping.key, owner );
        if ( ImGui::IsKeyPressed( mapping.key ) ) {
            actions.terminalInputs.push_back( makeInput( mapping.terminalKey, 0 ) );
        }
    }

    if ( io.KeyCtrl ) {
        for ( int key = ImGuiKey_A; key <= ImGuiKey_Z; key++ ) {
            if ( !ImGui::IsKeyPressed( static_cast<ImGuiKey>( key ) ) ) {
                continue;
            }
            char32_t letter = U'a' + ( key - ImGuiKey_A );
            if ( io.KeyShift && letter == U'v' ) {
                pasteClipboard( actions );
            } else if ( !io.KeyShift ) {
                actions.terminalInputs.push_back( makeInput( TerminalKey::None, letter ) );
            }
        }
        return;
    }
    for ( ImWchar character : io.InputQueueCharacters ) {
        if ( character >= U' ' ) {
            TerminalInput input = makeInput( TerminalKey::None, character );
            input.shift = false; // already applied to the character
            actions.terminalInputs.push_back( input );
        }
    }
}

static ImU32 cellColor( uint32_t rgb ) {
    return IM_COL32( ( rgb >> 16 ) & 0xff, ( rgb >> 8 ) & 0xff, rgb & 0xff, 255 );
}

static void drawCell( ImDrawList *drawList, const TerminalCell &cell, ImVec2 topLeft,
                      ImVec2 cellSize, bool selected ) {
    ImU32 foreground = cell.defaultForeground ? codeTextColor : cellColor( cell.foreground );
    ImU32 background = cell.defaultBackground ? transparentColor : cellColor( cell.background );
    if ( cell.reverse ) {
        ImU32 reversedForeground = cell.defaultBackground ? islandColor : background;
        background = foreground;
        foreground = reversedForeground;
    }
    if ( selected ) {
        background = textSelectionColor;
    }

    ImVec2 bottomRight( topLeft.x + cellSize.x * std::max( cell.width, 1 ),
                        topLeft.y + cellSize.y );
    if ( background != transparentColor ) {
        drawList->AddRectFilled( topLeft, bottomRight, background );
    }
    if ( cell.character > U' ' ) {
        char text[5] = {};
        ImTextCharToUtf8( text, cell.character );
        drawList->AddText( topLeft, foreground, text );
    }
    if ( cell.underline ) {
        drawList->AddLine( ImVec2( topLeft.x, bottomRight.y - 1.0f ),
                           ImVec2( bottomRight.x, bottomRight.y - 1.0f ), foreground );
    }
}

static void drawCursor( ImDrawList *drawList, const Terminal &terminal, ImVec2 origin,
                        ImVec2 cellSize ) {
    int row, column;
    terminalCursor( terminal, row, column );
    ImVec2 topLeft( origin.x + column * cellSize.x, origin.y + row * cellSize.y );
    ImVec2 bottomRight( topLeft.x + cellSize.x, topLeft.y + cellSize.y );
    if ( !focused ) {
        drawList->AddRect( topLeft, bottomRight, textColor );
        return;
    }
    drawList->AddRectFilled( topLeft, bottomRight, textColor );
    TerminalCell cell = terminalCell( terminal, row, column );
    if ( cell.character > U' ' ) {
        char text[5] = {};
        ImTextCharToUtf8( text, cell.character );
        drawList->AddText( topLeft, islandColor, text );
    }
}

static void selectionRange( CellPosition &start, CellPosition &end ) {
    start = std::min( selectionAnchor, selectionEnd );
    end = std::max( selectionAnchor, selectionEnd );
}

static bool isSelected( int line, int column ) {
    if ( !hasSelection ) {
        return false;
    }
    CellPosition start, end;
    selectionRange( start, end );
    CellPosition cell = { line, column };
    return cell >= start && cell < end;
}

static CellPosition positionAt( const Terminal &terminal, ImVec2 origin, ImVec2 cellSize ) {
    ImVec2 mouse = ImGui::GetMousePos();
    int row = static_cast<int>( std::floor( ( mouse.y - origin.y ) / cellSize.y ) );
    int column = static_cast<int>( std::round( ( mouse.x - origin.x ) / cellSize.x ) );
    int firstLine = -static_cast<int>( terminal.scrollback.size() );
    return { std::clamp( row - scrollOffset, firstLine, terminal.rows - 1 ),
             std::clamp( column, 0, terminal.columns ) };
}

static bool isWordCell( const Terminal &terminal, int line, int column ) {
    char32_t character = terminalCell( terminal, line, column ).character;
    return character > U' ';
}

static void selectWord( const Terminal &terminal, CellPosition position ) {
    int column = std::min( position.column, terminal.columns - 1 );
    if ( !isWordCell( terminal, position.line, column ) ) {
        hasSelection = false;
        return;
    }
    int start = column;
    while ( start > 0 && isWordCell( terminal, position.line, start - 1 ) ) {
        start--;
    }
    int end = column + 1;
    while ( end < terminal.columns && isWordCell( terminal, position.line, end ) ) {
        end++;
    }
    selectionAnchor = { position.line, start };
    selectionEnd = { position.line, end };
    hasSelection = true;
}

// Click and drag selects, a double click selects a word, a plain click clears.
static void updateSelection( const Terminal &terminal, ImVec2 origin, ImVec2 cellSize ) {
    if ( terminal.scrollback.size() < selectionScrollbackSize ) {
        hasSelection = false;
    } else if ( hasSelection ) {
        int pushedLines = static_cast<int>( terminal.scrollback.size() - selectionScrollbackSize );
        selectionAnchor.line -= pushedLines;
        selectionEnd.line -= pushedLines;
    }
    selectionScrollbackSize = terminal.scrollback.size();

    CellPosition position = positionAt( terminal, origin, cellSize );
    if ( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
        selecting = false;
        selectWord( terminal, position );
    } else if ( ImGui::IsItemClicked() ) {
        selecting = true;
        hasSelection = false;
        selectionAnchor = position;
        selectionEnd = position;
    } else if ( selecting && ImGui::IsMouseDown( ImGuiMouseButton_Left ) ) {
        selectionEnd = position;
        hasSelection = selectionAnchor != selectionEnd;
    } else {
        selecting = false;
    }
}

// Lines of the selection with their trailing spaces removed.
static std::string selectedText( const Terminal &terminal ) {
    CellPosition start, end;
    selectionRange( start, end );
    std::string text;
    for ( int line = start.line; line <= end.line; line++ ) {
        int first = line == start.line ? start.column : 0;
        int last = line == end.line ? end.column : terminal.columns;
        std::string lineText;
        for ( int column = first; column < last; column++ ) {
            char32_t character = terminalCell( terminal, line, column ).character;
            if ( character == 0 && column > first &&
                 terminalCell( terminal, line, column - 1 ).width > 1 ) {
                continue;
            }
            char utf8[5] = {};
            ImTextCharToUtf8( utf8, character ? character : U' ' );
            lineText += utf8;
        }
        lineText.erase( lineText.find_last_not_of( ' ' ) + 1 );
        text += lineText;
        if ( line != end.line ) {
            text += '\n';
        }
    }
    return text;
}

static void copySelection( const Terminal &terminal ) {
    if ( hasSelection ) {
        ImGui::SetClipboardText( selectedText( terminal ).c_str() );
    }
}

static void drawContextMenu( const Terminal &terminal, MenuActions &actions ) {
    if ( !ImGui::BeginPopupContextItem( "##terminalMenu" ) ) {
        return;
    }
    if ( ImGui::MenuItem( "Copy", "Ctrl+Shift+C", false, hasSelection ) ) {
        copySelection( terminal );
    }
    if ( ImGui::MenuItem( "Paste", "Ctrl+Shift+V" ) ) {
        pasteClipboard( actions );
    }
    ImGui::EndPopup();
}

void drawTerminalView( const Session &session, MenuActions &actions ) {
    const Terminal &terminal = session.terminal;
    pushCodeFont();
    ImVec2 cellSize( ImGui::CalcTextSize( "M" ).x, ImGui::GetTextLineHeight() );
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 area = ImGui::GetContentRegionAvail();
    area.x = std::max( area.x, cellSize.x );
    area.y = std::max( area.y, cellSize.y );
    actions.terminalRows = std::max( 1, static_cast<int>( area.y / cellSize.y ) );
    actions.terminalColumns = std::max( 2, static_cast<int>( area.x / cellSize.x ) );

    ImGui::InvisibleButton( "##terminal", area );
    ImGuiID viewId = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();
    if ( ImGui::IsItemClicked() ) {
        focused = true;
    } else if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && !hovered ) {
        focused = false;
    }
    if ( hovered ) {
        ImGui::SetMouseCursor( ImGuiMouseCursor_TextInput );
        scrollOffset += static_cast<int>( ImGui::GetIO().MouseWheel * wheelLines );
    }
    updateSelection( terminal, origin, cellSize );
    drawContextMenu( terminal, actions );
    if ( focused ) {
        if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_C ) ) {
            copySelection( terminal );
        }
        readKeyboard( viewId, actions );
    }
    if ( !actions.terminalInputs.empty() || !actions.terminalPaste.empty() ) {
        scrollOffset = 0;
        hasSelection = false;
    }
    scrollOffset = std::clamp( scrollOffset, 0, static_cast<int>( terminal.scrollback.size() ) );

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    for ( int row = 0; row < terminal.rows; row++ ) {
        for ( int column = 0; column < terminal.columns; column++ ) {
            ImVec2 topLeft( origin.x + column * cellSize.x, origin.y + row * cellSize.y );
            int line = row - scrollOffset;
            drawCell( drawList, terminalCell( terminal, line, column ), topLeft, cellSize,
                      isSelected( line, column ) );
        }
    }
    if ( scrollOffset == 0 && isRunning( terminal ) && terminal.cursorVisible ) {
        drawCursor( drawList, terminal, origin, cellSize );
    }
    if ( terminal.exited ) {
        std::string text = "Shell exited with code " + std::to_string( terminal.exitCode ) +
                           ". Press any key to start a new one.";
        ImVec2 bottomLeft( origin.x, origin.y + area.y - cellSize.y );
        drawList->AddText( bottomLeft, mutedTextColor, text.c_str() );
    }
    ImGui::PopFont();
}
