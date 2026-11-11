#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <cstdlib>
#include <iterator>

#include <app/session.h>
#include <gui/editor.h>
#include <gui/menu.h>
#include <gui/panels.h>
#include <gui/theme.h>
#include <gui/widgets.h>
#include <gui/window.h>

static const uint64_t memoryRowSize = 8;
static const uint64_t memoryRowCount = 64;

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

// Panels draw their own header row, so the dock tab bar is hidden.
static void beginPanel( const char *name, ImGuiWindowFlags flags = 0 ) {
    ImGuiWindowClass withoutDockTabBar;
    withoutDockTabBar.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass( &withoutDockTabBar );

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

static void drawPanelTitle( const char *title ) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( title );
}

// Header buttons as tall as the header row.
static void pushHeaderButtonPadding() {
    float padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( padding, padding ) );
}

static float headerButtonWidth() {
    return ImGui::GetFrameHeight();
}

static void drawEditor() {
    beginPanel( "Editor" );
    drawCodeEditor();
    ImGui::End();
}

enum class OutputTab { Console, Build };

static OutputTab shownOutputTab = OutputTab::Console;
static int shownBuildCount = 0;

// After a build, a failed one shows its errors; a good one shows the program.
static void followLatestBuild( const Session &session ) {
    if ( session.buildCount == shownBuildCount ) {
        return;
    }
    shownBuildCount = session.buildCount;
    shownOutputTab = session.buildSucceeded ? OutputTab::Console : OutputTab::Build;
}

static void drawOutputTab( const char *id, const char *label, OutputTab tab, bool hasIcon ) {
    PillTab pill;
    pill.label = label;
    pill.icon = Icon::Console;
    pill.hasIcon = hasIcon;
    pill.selected = shownOutputTab == tab;
    if ( drawPillTab( id, pill ) == TabClick::Select ) {
        shownOutputTab = tab;
    }
}

static void drawDebugButtons( const Session &session, MenuActions &actions ) {
    bool paused = session.isPaused();
    actions.debugContinue |= iconButton( "##resume", Icon::Resume, "Resume  F8", paused );
    ImGui::SameLine();
    actions.debugStep |= iconButton( "##step", Icon::StepOver, "Step  F10", paused );
    ImGui::SameLine();
    actions.stop |= iconButton( "##stop", Icon::Stop, "Stop  Shift+F5", session.isProgramActive() );
}

static void drawConsoleHeader( const Session &session, MenuActions &actions ) {
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing,
                         ImVec2( 2.0f, ImGui::GetStyle().ItemSpacing.y ) );
    drawOutputTab( "##consoleTab", "Console", OutputTab::Console, true );
    ImGui::SameLine();
    drawOutputTab( "##buildTab", "Build", OutputTab::Build, false );
    ImGui::SameLine();
    drawVerticalSeparator();
    ImGui::SameLine();

    pushHeaderButtonPadding();
    drawDebugButtons( session, actions );
    ImGui::SameLine( ImGui::GetWindowWidth() - headerButtonWidth() -
                     ImGui::GetStyle().WindowPadding.x );
    actions.clearOutput |= iconButton( "##clear", Icon::Clear, "Clear" );
    ImGui::PopStyleVar( 2 );
}

static void drawMessages( const std::vector<Message> &messages ) {
    ImGui::BeginChild( "##messages", ImVec2( 0.0f, 0.0f ), ImGuiChildFlags_None,
                       ImGuiWindowFlags_HorizontalScrollbar );
    pushCodeFont();
    bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

    ImGuiListClipper clipper;
    clipper.Begin( messages.size() );
    while ( clipper.Step() ) {
        for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ ) {
            const Message &message = messages[i];
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
    ImGui::EndChild();
}

static void drawConsole( const Session &session, MenuActions &actions ) {
    followLatestBuild( session );
    beginPanel( "Console" );
    drawConsoleHeader( session, actions );
    bool showsBuild = shownOutputTab == OutputTab::Build;
    drawMessages( showsBuild ? session.buildLog : session.console );
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
    drawPanelTitle( "Registers" );
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
    drawPanelTitle( "Memory" );
    float inputWidth = ImGui::GetContentRegionAvail().x * 0.45f;
    float labelWidth = ImGui::CalcTextSize( "Address" ).x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine( ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - inputWidth -
                     labelWidth );
    ImGui::TextDisabled( "Address" );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( inputWidth );
    ImGui::InputTextWithHint( "##address", "x1, sp, 0x4100d8", memoryAddressInput,
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

void drawPanels( Session &session, MenuActions &actions ) {
    syncEditorWithSession( session );
    drawDockSpace();
    drawEditor();
    drawConsole( session, actions );
    drawRegisters( session );
    drawMemory( session );
}
