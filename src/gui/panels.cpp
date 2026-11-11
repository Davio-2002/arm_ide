#include <TextEditor.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <set>

#include <app/session.h>
#include <gui/aarch64_language.h>
#include <gui/panels.h>
#include <gui/theme.h>
#include <gui/window.h>

static const uint64_t memoryRowSize = 8;
static const uint64_t memoryRowCount = 64;

static TextEditor editor;
static size_t savedUndoIndex = 0;
static int shownLineMarksVersion = -1;
static Session *editorSession = nullptr;
static char memoryAddressInput[64] = "sp";

static ImU32 messageColor( MessageKind kind ) {
    switch ( kind ) {
    case MessageKind::Command:
        return commandColor;
    case MessageKind::Error:
        return errorColor;
    case MessageKind::Warning:
        return warningColor;
    case MessageKind::Success:
        return successColor;
    case MessageKind::Output:
        break;
    }
    return codeTextColor;
}

static void addLineMarker( const LineMark &mark ) {
    ImU32 numberColor = errorColor;
    ImU32 lineColor = errorLineColor;
    if ( mark.kind == LineMarkKind::Warning ) {
        numberColor = warningColor;
        lineColor = warningLineColor;
    } else if ( mark.kind == LineMarkKind::Paused ) {
        numberColor = pausedColor;
        lineColor = pausedLineColor;
    }
    editor.AddMarker( mark.line - 1, numberColor, lineColor, mark.message, mark.message );
}

static void drawLineDecoration( TextEditor::Decorator &decorator ) {
    Session &session = *editorSession;
    int line = static_cast<int>( decorator.line ) + 1;
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( "##breakpoint", ImVec2( decorator.width, decorator.height ) ) ) {
        session.toggleBreakpoint( line );
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float size = decorator.height;
    ImVec2 center( topLeft.x + size * 0.5f, topLeft.y + size * 0.5f );
    float radius = size * 0.3f;
    if ( session.breakpoints.contains( line ) ) {
        drawList->AddCircleFilled( center, radius, breakpointColor );
    } else if ( ImGui::IsItemHovered() ) {
        drawList->AddCircleFilled( center, radius, breakpointHoverColor );
    }

    if ( line == session.pausedLine ) {
        float arrow = radius * 0.7f;
        ImVec2 top( center.x - arrow, center.y - arrow );
        ImVec2 bottom( center.x - arrow, center.y + arrow );
        ImVec2 tip( center.x + arrow, center.y );
        drawList->AddTriangleFilled( top, bottom, tip, pausedColor );
    }
}

static bool startsWithIgnoringCase( const std::string &word, const std::string &prefix ) {
    return word.size() >= prefix.size() &&
           std::equal( prefix.begin(), prefix.end(), word.begin(), []( char a, char b ) {
               return std::tolower( static_cast<unsigned char>( a ) ) ==
                      std::tolower( static_cast<unsigned char>( b ) );
           } );
}

static void suggestCompletions( TextEditor::AutoCompleteState &state ) {
    const TextEditor::Language &language = *aarch64Language();
    std::set<std::string> words;
    words.insert( language.keywords.begin(), language.keywords.end() );
    words.insert( language.declarations.begin(), language.declarations.end() );
    words.insert( language.identifiers.begin(), language.identifiers.end() );
    editor.IterateIdentifiers( [&words]( const std::string &word ) { words.insert( word ); } );

    state.suggestions.clear();
    for ( const std::string &word : words ) {
        if ( word.size() > state.searchTerm.size() &&
             startsWithIgnoringCase( word, state.searchTerm ) ) {
            state.suggestions.push_back( word );
        }
    }
    std::ranges::stable_sort( state.suggestions, []( const std::string &a, const std::string &b ) {
        return a.size() < b.size();
    } );
}

void setupEditor( Session &session ) {
    editorSession = &session;
    editor.SetPalette( editorPalette() );
    editor.SetLanguage( aarch64Language() );
    editor.SetTabSize( 8 );
    editor.SetShowWhitespacesEnabled( false );
    editor.SetLineDecorator( 2, drawLineDecoration );

    TextEditor::AutoCompleteConfig autoComplete;
    autoComplete.triggerCharacters = ".";
    autoComplete.wordCharacters = ".";
    autoComplete.callback = suggestCompletions;
    editor.SetAutoCompleteConfig( &autoComplete );
}

void setEditorText( const std::string &text ) {
    editor.SetText( text );
    markEditorSaved();
}

std::string editorText() {
    return editor.GetText();
}

void markEditorSaved() {
    savedUndoIndex = editor.GetUndoIndex();
}

bool isEditorModified() {
    return editor.GetUndoIndex() != savedUndoIndex;
}

int editorCursorLine() {
    return static_cast<int>( editor.GetCurrentCursorPosition().line ) + 1;
}

static void jumpToSourceLine( int sourceLine ) {
    size_t line = sourceLine - 1;
    editor.SetCursor( TextEditor::DocPos( line, 0 ) );
    editor.ScrollToLine( line );
    ImGui::SetWindowFocus( "Editor" );
}

static void syncEditorWithSession( Session &session ) {
    editor.SetReadOnlyEnabled( session.isDebugging() );

    if ( shownLineMarksVersion != session.lineMarksVersion ) {
        shownLineMarksVersion = session.lineMarksVersion;
        editor.ClearMarkers();
        for ( const LineMark &mark : session.lineMarks ) {
            addLineMarker( mark );
        }
    }

    if ( session.lineToReveal > 0 ) {
        editor.SetCursor( TextEditor::DocPos( session.lineToReveal - 1, 0 ) );
        session.lineToReveal = 0;
    }
}

static void createDefaultLayout( ImGuiID dockspace ) {
    ImGui::DockBuilderAddNode( dockspace, ImGuiDockNodeFlags_DockSpace );
    ImGui::DockBuilderSetNodeSize( dockspace, ImGui::GetMainViewport()->WorkSize );

    ImGuiID leftNode, rightNode;
    ImGui::DockBuilderSplitNode( dockspace, ImGuiDir_Right, 0.3f, &rightNode, &leftNode );
    ImGuiID editorNode, consoleNode;
    ImGui::DockBuilderSplitNode( leftNode, ImGuiDir_Down, 0.25f, &consoleNode, &editorNode );
    ImGuiID registersNode, memoryNode;
    ImGui::DockBuilderSplitNode( rightNode, ImGuiDir_Down, 0.4f, &memoryNode, &registersNode );

    ImGui::DockBuilderDockWindow( "Editor", editorNode );
    ImGui::DockBuilderDockWindow( "Console", consoleNode );
    ImGui::DockBuilderDockWindow( "Registers", registersNode );
    ImGui::DockBuilderDockWindow( "Memory", memoryNode );
    ImGui::DockBuilderFinish( dockspace );
}

// The panels are islands: the ground window and dock title bars stay transparent, and each
// docked panel draws its rounded background behind everything.
static void drawDockSpace() {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( viewport->WorkPos );
    ImGui::SetNextWindowSize( viewport->WorkSize );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( islandGap, islandGap ) );
    ImGuiWindowFlags groundFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin( "##Ground", nullptr, groundFlags );
    ImGui::PopStyleVar();

    ImGuiID dockspace = ImGui::GetID( "MainDockSpace v3" );
    if ( !ImGui::DockBuilderGetNode( dockspace ) ) {
        createDefaultLayout( dockspace );
    }
    ImGuiCol hiddenColors[] = { ImGuiCol_TitleBg,          ImGuiCol_TitleBgActive,
                                ImGuiCol_TitleBgCollapsed, ImGuiCol_Border,
                                ImGuiCol_DockingEmptyBg,   ImGuiCol_WindowBg };
    for ( ImGuiCol color : hiddenColors ) {
        ImGui::PushStyleColor( color, transparentColor );
    }
    ImGui::DockSpace( dockspace );
    ImGui::PopStyleColor( std::size( hiddenColors ) );
    ImGui::End();
}

static void beginPanel( const char *name, ImGuiWindowFlags flags = 0 ) {
    ImGuiWindow *window = ImGui::FindWindowByName( name );
    bool docked = !window || window->DockNode != nullptr;
    if ( docked ) {
        ImGui::PushStyleColor( ImGuiCol_WindowBg, transparentColor );
    }
    ImGui::Begin( name, nullptr, flags );
    if ( docked ) {
        ImGui::PopStyleColor();
    }

    if ( ImGuiDockNode *node = ImGui::GetWindowDockNode() ) {
        ImVec2 islandEnd( node->Pos.x + node->Size.x, node->Pos.y + node->Size.y );
        ImGui::GetBackgroundDrawList()->AddRectFilled( node->Pos, islandEnd, islandColor,
                                                       islandRounding );
    }
}

static void drawEditor() {
    beginPanel( "Editor" );
    pushCodeFont();
    editor.Render( "##code" );
    ImGui::PopFont();
    ImGui::End();
}

static void drawConsole( const Session &session ) {
    beginPanel( "Console", ImGuiWindowFlags_HorizontalScrollbar );
    pushCodeFont();
    bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

    ImGuiListClipper clipper;
    clipper.Begin( session.console.size() );
    while ( clipper.Step() ) {
        for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ ) {
            const Message &message = session.console[i];
            ImGui::PushID( i );
            ImGui::PushStyleColor( ImGuiCol_Text, messageColor( message.kind ) );
            bool clicked = ImGui::Selectable( message.text.c_str() );
            ImGui::PopStyleColor();
            ImGui::PopID();

            if ( clicked && message.sourceLine > 0 ) {
                jumpToSourceLine( message.sourceLine );
            }
        }
    }

    if ( wasAtBottom ) {
        ImGui::SetScrollHereY( 1.0f );
    }
    ImGui::PopFont();
    ImGui::End();
}

static ImVec4 changeColor( bool changed ) {
    return ImGui::ColorConvertU32ToFloat4( changed ? changedValueColor : codeTextColor );
}

static void drawRegisterRow( const std::string &name, uint64_t value, uint64_t previousValue ) {
    ImVec4 color = changeColor( value != previousValue );
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled( "%s", name.c_str() );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "0x%016llx", static_cast<unsigned long long>( value ) );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "%lld", static_cast<long long>( value ) );
}

static void drawFlags( const Registers &registers, const Registers &previous ) {
    const char *names[] = { "N", "Z", "C", "V" };
    for ( int i = 0; i < 4; i++ ) {
        int bit = 31 - i;
        bool value = ( registers.nzcv >> bit ) & 1;
        bool previousValue = ( previous.nzcv >> bit ) & 1;
        ImGui::TextColored( changeColor( value != previousValue ), "%s=%d", names[i], value );
        ImGui::SameLine( 0.0f, 16.0f );
    }
    ImGui::NewLine();
}

static void drawRegisters( const Session &session ) {
    beginPanel( "Registers" );
    if ( !session.hasRegisters ) {
        ImGui::TextDisabled( "Start debugging with F10 or F8" );
        ImGui::End();
        return;
    }

    const Registers &registers = session.registers;
    const Registers &previous = session.previousRegisters;
    pushCodeFont();
    drawFlags( registers, previous );
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if ( ImGui::BeginTable( "registers", 3, flags ) ) {
        for ( int i = 0; i < 31; i++ ) {
            std::string name = "x" + std::to_string( i );
            if ( i == 29 ) {
                name += " fp";
            } else if ( i == 30 ) {
                name += " lr";
            }
            drawRegisterRow( name, registers.x[i], previous.x[i] );
        }
        drawRegisterRow( "sp", registers.sp, previous.sp );
        drawRegisterRow( "pc", registers.pc, previous.pc );
        ImGui::EndTable();
    }
    ImGui::PopFont();
    ImGui::End();
}

static bool parseAddress( const std::string &text, const Registers &registers, uint64_t &address ) {
    if ( text == "sp" ) {
        address = registers.sp;
        return true;
    }
    if ( text == "pc" ) {
        address = registers.pc;
        return true;
    }

    const char *start = text.c_str();
    bool isRegister = text.starts_with( 'x' );
    if ( isRegister ) {
        start++;
    }
    char *end = nullptr;
    uint64_t value = std::strtoull( start, &end, isRegister ? 10 : 0 );
    if ( end == start || *end != '\0' ) {
        return false;
    }

    if ( !isRegister ) {
        address = value;
        return true;
    }
    if ( value > 30 ) {
        return false;
    }
    address = registers.x[value];
    return true;
}

static void drawMemoryRow( const Session &session, uint64_t rowAddress ) {
    std::string hex;
    std::string text;
    char byteText[4];
    for ( uint64_t i = 0; i < memoryRowSize; i++ ) {
        uint8_t byte = 0;
        if ( session.readMemory( rowAddress + i, byte ) ) {
            std::snprintf( byteText, sizeof( byteText ), "%02x ", byte );
            hex += byteText;
            text += byte >= 32 && byte < 127 ? static_cast<char>( byte ) : '.';
        } else {
            hex += "?? ";
            text += ' ';
        }
    }
    ImGui::TextDisabled( "%08llx", static_cast<unsigned long long>( rowAddress ) );
    ImGui::SameLine();
    ImGui::Text( "%s %s", hex.c_str(), text.c_str() );
}

static void drawMemory( const Session &session ) {
    beginPanel( "Memory", ImGuiWindowFlags_HorizontalScrollbar );
    ImGui::SetNextItemWidth( -1 );
    ImGui::InputTextWithHint( "##address", "address, register or sp", memoryAddressInput,
                              sizeof( memoryAddressInput ) );

    uint64_t address = 0;
    if ( !session.isPaused() ) {
        ImGui::TextDisabled( "Pause the program to read memory" );
    } else if ( !parseAddress( memoryAddressInput, session.registers, address ) ) {
        ImGui::TextDisabled( "Type an address like 0x4100d8, or a register like x1 or sp" );
    } else {
        pushCodeFont();
        uint64_t firstRow = address / memoryRowSize * memoryRowSize;
        for ( uint64_t row = 0; row < memoryRowCount; row++ ) {
            drawMemoryRow( session, firstRow + row * memoryRowSize );
        }
        ImGui::PopFont();
    }
    ImGui::End();
}

void drawPanels( Session &session ) {
    syncEditorWithSession( session );
    drawDockSpace();
    drawEditor();
    drawConsole( session );
    drawRegisters( session );
    drawMemory( session );
}
