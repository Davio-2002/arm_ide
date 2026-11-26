#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <app/session/session.h>
#include <core/terminal.h>
#include <gui/look/fonts.h>
#include <gui/look/theme.h>
#include <gui/panels/terminal_view.h>

namespace {

const int wheelLines = 3;
const auto busyTime = std::chrono::milliseconds( 500 );

/// @brief Keys and pasted text of this frame, sent in order.
struct TypedInput {
    std::vector<TerminalInput> inputs;
    std::string paste;
};

struct KeyMapping {
    ImGuiKey key;
    TerminalKey terminalKey;
};

const auto specialKeys = std::to_array<KeyMapping>( {
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
} );

void pasteClipboard( TypedInput &typed ) {
    const char *clipboard = ImGui::GetClipboardText();
    typed.paste += clipboard ? clipboard : "";
}

/// @brief Input with the modifiers held now.
/// @param[in] character Used when key is None.
TerminalInput makeInput( TerminalKey key, char32_t character ) {
    const ImGuiIO &io = ImGui::GetIO();
    TerminalInput input;
    input.key = key;
    input.character = character;
    input.shift = io.KeyShift;
    input.ctrl = io.KeyCtrl;
    input.alt = io.KeyAlt;
    return input;
}

/// @brief Reads this frame's input. Owns the navigation keys so Tab, arrows and Escape reach
///        the shell; Ctrl+letter is a key, other text characters; Ctrl+Shift+V pastes.
/// @param[in] owner ImGui ID of the view.
void readKeyboard( ImGuiID owner, TypedInput &typed ) {
    const ImGuiIO &io = ImGui::GetIO();
    for ( const KeyMapping &mapping : specialKeys ) {
        ImGui::SetKeyOwner( mapping.key, owner );
        if ( ImGui::IsKeyPressed( mapping.key ) ) {
            typed.inputs.push_back( makeInput( mapping.terminalKey, 0 ) );
        }
    }

    if ( io.KeyCtrl ) {
        for ( int key = ImGuiKey_A; key <= ImGuiKey_Z; key++ ) {
            if ( !ImGui::IsKeyPressed( static_cast<ImGuiKey>( key ) ) ) {
                continue;
            }
            char32_t const letter = U'a' + ( key - ImGuiKey_A );
            if ( io.KeyShift && letter == U'v' ) {
                pasteClipboard( typed );
            } else if ( !io.KeyShift ) {
                typed.inputs.push_back( makeInput( TerminalKey::None, letter ) );
            }
        }
        return;
    }
    for ( ImWchar const character : io.InputQueueCharacters ) {
        if ( character >= U' ' ) {
            TerminalInput input = makeInput( TerminalKey::None, character );
            input.shift = false;
            typed.inputs.push_back( input );
        }
    }
}

/// @return Opaque ImGui colour of 0xRRGGBB.
ImU32 cellColor( uint32_t rgb ) {
    return IM_COL32( ( rgb >> 16 ) & 0xff, ( rgb >> 8 ) & 0xff, rgb & 0xff, 255 );
}

/// @brief Background, character and underline, with reverse video applied.
void drawCell( ImDrawList *drawList, const TerminalCell &cell, ImVec2 topLeft, ImVec2 cellSize,
               bool selected ) {
    ImU32 foreground =
        cell.defaultForeground ? currentScheme().codeText : cellColor( cell.foreground );
    ImU32 background = cell.defaultBackground ? transparentColor : cellColor( cell.background );
    if ( cell.reverse ) {
        ImU32 const reversedForeground =
            cell.defaultBackground ? currentScheme().background : background;
        background = foreground;
        foreground = reversedForeground;
    }
    if ( selected ) {
        background = currentScheme().textSelection;
    }

    ImVec2 const bottomRight( topLeft.x +
                                  cellSize.x * static_cast<float>( std::max( cell.width, 1 ) ),
                              topLeft.y + cellSize.y );
    if ( background != transparentColor ) {
        drawList->AddRectFilled( topLeft, bottomRight, background );
    }
    if ( cell.character > U' ' ) {
        std::array<char, 5> text{};
        ImTextCharToUtf8( text.data(), cell.character );
        drawList->AddText( topLeft, foreground, text.data() );
    }
    if ( cell.underline ) {
        drawList->AddLine( ImVec2( topLeft.x, bottomRight.y - 1.0f ),
                           ImVec2( bottomRight.x, bottomRight.y - 1.0f ), foreground );
    }
}

/// @brief Filled block while focused, outline otherwise.
void drawCursor( ImDrawList *drawList, const TerminalView &view, ImVec2 origin, ImVec2 cellSize ) {
    const Terminal &terminal = view.terminal;
    int row, column;
    terminalCursor( terminal, row, column );
    ImVec2 const topLeft( origin.x + ( static_cast<float>( column ) * cellSize.x ),
                          origin.y + ( static_cast<float>( row ) * cellSize.y ) );
    ImVec2 const bottomRight( topLeft.x + cellSize.x, topLeft.y + cellSize.y );
    if ( !view.focused ) {
        drawList->AddRect( topLeft, bottomRight, currentScheme().text );
        return;
    }
    drawList->AddRectFilled( topLeft, bottomRight, currentScheme().text );
    TerminalCell const cell = terminalCell( terminal, row, column );
    if ( cell.character > U' ' ) {
        std::array<char, 5> text{};
        ImTextCharToUtf8( text.data(), cell.character );
        drawList->AddText( topLeft, currentScheme().background, text.data() );
    }
}

/// @brief Selection ends in order.
void selectionRange( const TerminalView &view, CellPosition &start, CellPosition &end ) {
    start = std::min( view.selectionAnchor, view.selectionEnd );
    end = std::max( view.selectionAnchor, view.selectionEnd );
}

/// @param[in] line Screen row; negative for scrollback.
bool isSelected( const TerminalView &view, int line, int column ) {
    if ( !view.hasSelection ) {
        return false;
    }
    CellPosition start, end;
    selectionRange( view, start, end );
    CellPosition const cell = { line, column };
    return cell >= start && cell < end;
}

/// @return Place between cells nearest the mouse, clamped to the text, scroll applied.
CellPosition positionAt( const TerminalView &view, ImVec2 origin, ImVec2 cellSize ) {
    const Terminal &terminal = view.terminal;
    ImVec2 const mouse = ImGui::GetMousePos();
    int const row = static_cast<int>( std::floor( ( mouse.y - origin.y ) / cellSize.y ) );
    int const column = static_cast<int>( std::round( ( mouse.x - origin.x ) / cellSize.x ) );
    int const firstLine = -static_cast<int>( terminal.scrollback.size() );
    return { std::clamp( row - view.scrollOffset, firstLine, terminal.rows - 1 ),
             std::clamp( column, 0, terminal.columns ) };
}

/// @return True if the cell holds a visible character.
bool isWordCell( const Terminal &terminal, int line, int column ) {
    char32_t const character = terminalCell( terminal, line, column ).character;
    return character > U' ';
}

/// @brief Selects the word at a position, or clears on a blank.
void selectWord( TerminalView &view, CellPosition position ) {
    const Terminal &terminal = view.terminal;
    int const column = std::min( position.column, terminal.columns - 1 );
    if ( !isWordCell( terminal, position.line, column ) ) {
        view.hasSelection = false;
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
    view.selectionAnchor = { position.line, start };
    view.selectionEnd = { position.line, end };
    view.hasSelection = true;
}

/// @brief Drag selects, double click selects a word, click clears. The selection moves up
///        as output pushes lines into the scrollback.
void updateSelection( TerminalView &view, ImVec2 origin, ImVec2 cellSize ) {
    size_t const scrollbackSize = view.terminal.scrollback.size();
    if ( scrollbackSize < view.selectionScrollbackSize ) {
        view.hasSelection = false;
    } else if ( view.hasSelection ) {
        int const pushedLines = static_cast<int>( scrollbackSize - view.selectionScrollbackSize );
        view.selectionAnchor.line -= pushedLines;
        view.selectionEnd.line -= pushedLines;
    }
    view.selectionScrollbackSize = scrollbackSize;

    CellPosition const position = positionAt( view, origin, cellSize );
    if ( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
        view.selecting = false;
        selectWord( view, position );
    } else if ( ImGui::IsItemClicked() ) {
        view.selecting = true;
        view.hasSelection = false;
        view.selectionAnchor = position;
        view.selectionEnd = position;
    } else if ( view.selecting && ImGui::IsMouseDown( ImGuiMouseButton_Left ) ) {
        view.selectionEnd = position;
        view.hasSelection = view.selectionAnchor != view.selectionEnd;
    } else {
        view.selecting = false;
    }
}

/// @return UTF-8, lines without trailing spaces.
std::string selectedText( const TerminalView &view ) {
    const Terminal &terminal = view.terminal;
    CellPosition start, end;
    selectionRange( view, start, end );
    std::string text;
    for ( int line = start.line; line <= end.line; line++ ) {
        int const first = line == start.line ? start.column : 0;
        int const last = line == end.line ? end.column : terminal.columns;
        std::string lineText;
        for ( int column = first; column < last; column++ ) {
            char32_t const character = terminalCell( terminal, line, column ).character;
            if ( character == 0 && column > first &&
                 terminalCell( terminal, line, column - 1 ).width > 1 ) {
                continue;
            }
            std::array<char, 5> utf8{};
            ImTextCharToUtf8( utf8.data(), character ? character : U' ' );
            lineText += utf8.data();
        }
        lineText.erase( lineText.find_last_not_of( ' ' ) + 1 );
        text += lineText;
        if ( line != end.line ) {
            text += '\n';
        }
    }
    return text;
}

void copySelection( const TerminalView &view ) {
    if ( view.hasSelection ) {
        ImGui::SetClipboardText( selectedText( view ).c_str() );
    }
}

/// @brief Right-click menu with Copy and Paste.
void drawContextMenu( const TerminalView &view, TypedInput &typed ) {
    if ( !ImGui::BeginPopupContextItem( "##terminalMenu" ) ) {
        return;
    }
    if ( ImGui::MenuItem( "Copy", "Ctrl+Shift+C", false, view.hasSelection ) ) {
        copySelection( view );
    }
    if ( ImGui::MenuItem( "Paste", "Ctrl+Shift+V" ) ) {
        pasteClipboard( typed );
    }
    ImGui::EndPopup();
}

/// @return Active document's folder, or $HOME for an untitled one.
std::string startDirectory( const Session &session ) {
    std::string const directory =
        std::filesystem::path( session.activeDocument().path ).parent_path();
    const char *home = std::getenv( "HOME" );
    return directory.empty() && home ? home : directory;
}

/// @brief Starts the shell the first time and keeps the pty sized to the view.
void fitTerminal( TerminalView &view, const Session &session, int rows, int columns ) {
    Terminal &terminal = view.terminal;
    if ( !isRunning( terminal ) && !terminal.exited ) {
        startTerminal( terminal, startDirectory( session ), rows, columns );
        view.lastActivity = std::chrono::steady_clock::now();
    } else if ( rows != terminal.rows || columns != terminal.columns ) {
        resizeTerminal( terminal, rows, columns );
    }
}

/// @brief Sends this frame's input; a key after the shell exited starts a new one.
void sendTyped( TerminalView &view, const Session &session, const TypedInput &typed ) {
    if ( typed.inputs.empty() && typed.paste.empty() ) {
        return;
    }
    Terminal &terminal = view.terminal;
    view.lastActivity = std::chrono::steady_clock::now();
    if ( terminal.exited ) {
        startTerminal( terminal, startDirectory( session ), terminal.rows, terminal.columns );
        return;
    }
    for ( const TerminalInput &input : typed.inputs ) {
        sendTerminalInput( terminal, input );
    }
    if ( !typed.paste.empty() ) {
        pasteToTerminal( terminal, typed.paste );
    }
}

} // anonymous namespace

void focusTerminalView( TerminalView &view ) {
    view.focused = true;
}

void updateTerminalView( TerminalView &view ) {
    if ( updateTerminal( view.terminal ) ) {
        view.lastActivity = std::chrono::steady_clock::now();
    }
}

bool isTerminalBusy( const TerminalView &view ) {
    return std::chrono::steady_clock::now() - view.lastActivity < busyTime;
}

bool isTerminalRunning( const TerminalView &view ) {
    return isRunning( view.terminal );
}

void stopTerminalShell( TerminalView &view ) {
    stopTerminal( view.terminal );
}

void clearTerminalView( TerminalView &view ) {
    view.terminal.scrollback.clear();
    sendTerminalInput( view.terminal, { .character = U'l', .ctrl = true } );
    view.lastActivity = std::chrono::steady_clock::now();
}

void drawTerminalView( TerminalView &view, const Session &session ) {
    const Terminal &terminal = view.terminal;
    pushCodeFont();
    ImVec2 const cellSize( ImGui::CalcTextSize( "M" ).x, ImGui::GetTextLineHeight() );
    ImVec2 const origin = ImGui::GetCursorScreenPos();
    ImVec2 area = ImGui::GetContentRegionAvail();
    area.x = std::max( area.x, cellSize.x );
    area.y = std::max( area.y, cellSize.y );
    fitTerminal( view, session, std::max( 1, static_cast<int>( area.y / cellSize.y ) ),
                 std::max( 2, static_cast<int>( area.x / cellSize.x ) ) );
    TypedInput typed;

    ImGui::InvisibleButton( "##terminal", area );
    ImGuiID const viewId = ImGui::GetItemID();
    bool const hovered = ImGui::IsItemHovered();
    if ( ImGui::IsItemClicked() ) {
        view.focused = true;
    } else if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && !hovered ) {
        view.focused = false;
    }
    if ( hovered ) {
        ImGui::SetMouseCursor( ImGuiMouseCursor_TextInput );
        view.scrollOffset += static_cast<int>( ImGui::GetIO().MouseWheel * wheelLines );
    }
    updateSelection( view, origin, cellSize );
    drawContextMenu( view, typed );
    if ( view.focused ) {
        if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_C ) ) {
            copySelection( view );
        }
        readKeyboard( viewId, typed );
    }
    if ( !typed.inputs.empty() || !typed.paste.empty() ) {
        view.scrollOffset = 0;
        view.hasSelection = false;
    }
    sendTyped( view, session, typed );
    view.scrollOffset =
        std::clamp( view.scrollOffset, 0, static_cast<int>( terminal.scrollback.size() ) );

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    for ( int row = 0; row < terminal.rows; row++ ) {
        for ( int column = 0; column < terminal.columns; column++ ) {
            ImVec2 const topLeft( origin.x + ( static_cast<float>( column ) * cellSize.x ),
                                  origin.y + ( static_cast<float>( row ) * cellSize.y ) );
            int const line = row - view.scrollOffset;
            drawCell( drawList, terminalCell( terminal, line, column ), topLeft, cellSize,
                      isSelected( view, line, column ) );
        }
    }
    if ( view.scrollOffset == 0 && isRunning( terminal ) && terminal.cursorVisible ) {
        drawCursor( drawList, view, origin, cellSize );
    }
    if ( terminal.exited ) {
        std::string const text = "Shell exited with code " + std::to_string( terminal.exitCode ) +
                                 ". Press any key to start a new one.";
        ImVec2 const bottomLeft( origin.x, origin.y + area.y - cellSize.y );
        drawList->AddText( bottomLeft, currentScheme().mutedText, text.c_str() );
    }
    ImGui::PopFont();
}
