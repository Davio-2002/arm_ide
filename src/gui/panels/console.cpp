#include <imgui.h>

#include <app/session.h>
#include <gui/commands.h>
#include <gui/fonts.h>
#include <gui/panels/panels.h>
#include <gui/theme.h>
#include <gui/widgets.h>

namespace {

/// @return Text colour of the message.
ImU32 messageColor( MessageKind kind ) {
    switch ( kind ) {
    case MessageKind::Command:
        return currentScheme().mutedText;
    case MessageKind::Error:
        return currentScheme().error;
    case MessageKind::Warning:
        return currentScheme().warning;
    case MessageKind::Success:
        return currentScheme().success;
    case MessageKind::Output:
        break;
    }
    return currentScheme().codeText;
}

enum class OutputTab { Console, Build, Terminal };

OutputTab shownOutputTab = OutputTab::Console;
int shownBuildCount = 0;

/// @brief Switches tabs once after each build: a failed build shows its errors, a good one
///        shows the program.
void followLatestBuild( const Session &session ) {
    if ( session.buildCount == shownBuildCount ) {
        return;
    }
    shownBuildCount = session.buildCount;
    shownOutputTab = session.buildSucceeded ? OutputTab::Console : OutputTab::Build;
}

/// @brief Draws one tab of the header and selects it on click.
/// @param[in] label   Tab text.
/// @param[in] tab     Tab it selects.
/// @param[in] hasIcon True to show the console icon.
void drawOutputTab( const char *id, const char *label, OutputTab tab, bool hasIcon ) {
    PillTab pill;
    pill.label = label;
    pill.icon = Icon::Console;
    pill.hasIcon = hasIcon;
    pill.selected = shownOutputTab == tab;
    if ( drawPillTab( id, pill ) == TabClick::Select ) {
        shownOutputTab = tab;
        if ( tab == OutputTab::Terminal ) {
            focusTerminalView();
        }
    }
}

/// @brief Draws the Resume, Step and Stop buttons. Resume and Step only resume a paused
///        program; starting the debugger is the toolbar's.
void drawDebugButtons( const Session &session ) {
    bool paused = session.isPaused();
    if ( iconButton( "##resume", Icon::Resume, "Resume  F8", paused ) ) {
        requestCommand( Command::DebugContinue );
    }
    ImGui::SameLine();
    if ( iconButton( "##step", Icon::StepOver, "Step  F10", paused ) ) {
        requestCommand( Command::DebugStep );
    }
    ImGui::SameLine();
    commandButton( session, "##stop", Command::Stop, Icon::Stop );
}

/// @brief Draws the header row: tabs, then debug or shell buttons, and Clear on the right.
void drawConsoleHeader( const Session &session ) {
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing,
                         ImVec2( 2.0f, ImGui::GetStyle().ItemSpacing.y ) );
    drawOutputTab( "##consoleTab", "Console", OutputTab::Console, true );
    ImGui::SameLine();
    drawOutputTab( "##buildTab", "Build", OutputTab::Build, false );
    ImGui::SameLine();
    drawOutputTab( "##terminalTab", "Terminal", OutputTab::Terminal, false );
    ImGui::SameLine();
    drawVerticalSeparator();
    ImGui::SameLine();

    bool showsTerminal = shownOutputTab == OutputTab::Terminal;
    pushHeaderButtonPadding();
    if ( showsTerminal ) {
        if ( iconButton( "##stopShell", Icon::Stop, "Kill the shell and start a new one",
                         isTerminalRunning() ) ) {
            stopTerminalShell();
        }
    } else {
        drawDebugButtons( session );
    }
    ImGui::SameLine( ImGui::GetWindowWidth() - headerButtonWidth() -
                     ImGui::GetStyle().WindowPadding.x );
    if ( iconButton( "##clear", Icon::Clear, "Clear" ) ) {
        if ( showsTerminal ) {
            clearTerminalView();
        } else {
            requestCommand( Command::ClearOutput );
        }
    }
    ImGui::PopStyleVar( 2 );
}

/// @brief Selected lines of one output tab.
///
/// It holds indices into the tab's messages, so a shorter list clears it.
struct MessageSelection {
    ImGuiSelectionBasicStorage lines;
    size_t messageCount = 0;
};

MessageSelection consoleSelection;
MessageSelection buildSelection;

/// @brief Copies messages to the clipboard, one per line.
/// @param[in] selection Lines to copy, or nullptr to copy all.
void copyMessages( const std::vector<Message> &messages,
                   const ImGuiSelectionBasicStorage *selection ) {
    std::string text;
    for ( size_t i = 0; i < messages.size(); i++ ) {
        if ( !selection || selection->Contains( static_cast<ImGuiID>( i ) ) ) {
            text += messages[i].text + "\n";
        }
    }
    ImGui::SetClipboardText( text.c_str() );
}

/// @brief Handles Ctrl+C and the right-click menu with Copy and Copy All.
void drawCopyMenu( const std::vector<Message> &messages,
                   const ImGuiSelectionBasicStorage &selection ) {
    if ( ImGui::IsWindowFocused() && ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_C ) ) {
        copyMessages( messages, &selection );
    }
    if ( ImGui::BeginPopupContextWindow( "##copyMenu" ) ) {
        if ( ImGui::MenuItem( "Copy", "Ctrl+C", false, selection.Size > 0 ) ) {
            copyMessages( messages, &selection );
        }
        if ( ImGui::MenuItem( "Copy All" ) ) {
            copyMessages( messages, nullptr );
        }
        ImGui::EndPopup();
    }
}

/// @brief Draws messages as a selectable list that follows new output while scrolled to the
///        bottom.
///
/// Lines are selected like a list: click, Shift/Ctrl+click, drag or Ctrl+A. A click on a
/// message with a source line jumps to that line.
/// @param[in]     messages  Messages to draw.
/// @param[in,out] selection Selection of this list.
/// @param[in]     height    Height of the list; 0 to fill the panel, negative to leave room.
void drawMessages( const std::vector<Message> &messages, MessageSelection &selection,
                   float height = 0.0f ) {
    if ( messages.size() < selection.messageCount ) {
        selection.lines.Clear();
    }
    selection.messageCount = messages.size();

    ImGui::BeginChild( "##messages", ImVec2( 0.0f, height ), ImGuiChildFlags_None,
                       ImGuiWindowFlags_HorizontalScrollbar );
    pushCodeFont();
    bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

    ImGuiMultiSelectFlags selectFlags = ImGuiMultiSelectFlags_ClearOnEscape |
                                        ImGuiMultiSelectFlags_ClearOnClickVoid |
                                        ImGuiMultiSelectFlags_BoxSelect1d;
    int count = static_cast<int>( messages.size() );
    ImGuiMultiSelectIO *request =
        ImGui::BeginMultiSelect( selectFlags, selection.lines.Size, count );
    selection.lines.ApplyRequests( request );

    ImGuiListClipper clipper;
    clipper.Begin( count );
    if ( request->RangeSrcItem != -1 ) {
        clipper.IncludeItemByIndex( static_cast<int>( request->RangeSrcItem ) );
    }
    while ( clipper.Step() ) {
        for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ ) {
            const Message &message = messages[i];
            ImGui::PushID( i );
            ImGui::PushStyleColor( ImGuiCol_Text, messageColor( message.kind ) );
            ImGui::SetNextItemSelectionUserData( i );
            bool selected = selection.lines.Contains( static_cast<ImGuiID>( i ) );
            bool clicked = ImGui::Selectable( message.text.c_str(), selected );
            ImGui::PopStyleColor();
            ImGui::PopID();

            if ( clicked && message.sourceLine > 0 ) {
                int line = message.sourceLine;
                requestAction( [line]( Session &session ) { session.showProgramLine( line ); } );
                ImGui::SetWindowFocus( "Editor" );
            }
        }
    }
    selection.lines.ApplyRequests( ImGui::EndMultiSelect() );

    if ( wasAtBottom ) {
        ImGui::SetScrollHereY( 1.0f );
    }
    ImGui::PopFont();
    drawCopyMenu( messages, selection.lines );
    ImGui::EndChild();
}

char programInput[1024];

/// @brief Draws the input line for the running program's stdin. Enter sends the line,
///        Ctrl+D ends the input.
void drawProgramInput( const Session &session ) {
    bool enabled = session.acceptsInput();
    const char *hint = enabled ? "Input for the program: Enter sends a line, Ctrl+D ends input"
                               : "Input for the program, while it runs";
    ImGui::BeginDisabled( !enabled );
    ImGui::SetNextItemWidth( -FLT_MIN );
    bool entered =
        ImGui::InputTextWithHint( "##programInput", hint, programInput, sizeof( programInput ),
                                  ImGuiInputTextFlags_EnterReturnsTrue );
    if ( entered ) {
        std::string line = programInput;
        requestAction( [line]( Session &session ) { session.sendProgramInput( line ); } );
        programInput[0] = '\0';
        ImGui::SetKeyboardFocusHere( -1 );
    }
    if ( ImGui::IsItemActive() && ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_D ) ) {
        requestAction( []( Session &session ) { session.endProgramInput(); } );
    }
    ImGui::EndDisabled();
}

/// @brief Draws the program output with the input line below it.
void drawConsoleTab( const Session &session ) {
    pushCodeFont();
    float inputHeight = ImGui::GetFrameHeightWithSpacing();
    drawMessages( session.console, consoleSelection, -inputHeight );
    drawProgramInput( session );
    ImGui::PopFont();
}

} // anonymous namespace

void drawConsolePanel( const Session &session ) {
    followLatestBuild( session );
    beginPanel( "Console" );
    drawConsoleHeader( session );
    switch ( shownOutputTab ) {
    case OutputTab::Console:
        drawConsoleTab( session );
        break;
    case OutputTab::Build:
        drawMessages( session.buildLog, buildSelection );
        break;
    case OutputTab::Terminal:
        drawTerminalView( session );
        break;
    }
    ImGui::End();
}
