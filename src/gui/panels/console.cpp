#include <imgui.h>

#include <app/session.h>
#include <gui/menu.h>
#include <gui/panels/console.h>
#include <gui/panels/editor.h>
#include <gui/panels/panels.h>
#include <gui/panels/terminal_view.h>
#include <gui/theme.h>
#include <gui/widgets.h>
#include <gui/window.h>

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

enum class OutputTab { Console, Build, Terminal };

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
        if ( tab == OutputTab::Terminal ) {
            focusTerminalView();
        }
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
    drawOutputTab( "##terminalTab", "Terminal", OutputTab::Terminal, false );
    ImGui::SameLine();
    drawVerticalSeparator();
    ImGui::SameLine();

    bool showsTerminal = shownOutputTab == OutputTab::Terminal;
    pushHeaderButtonPadding();
    if ( showsTerminal ) {
        actions.stopTerminal |=
            iconButton( "##stopShell", Icon::Stop, "Kill the shell and start a new one",
                        isRunning( session.terminal ) );
    } else {
        drawDebugButtons( session, actions );
    }
    ImGui::SameLine( ImGui::GetWindowWidth() - headerButtonWidth() -
                     ImGui::GetStyle().WindowPadding.x );
    bool clear = iconButton( "##clear", Icon::Clear, "Clear" );
    ( showsTerminal ? actions.clearTerminal : actions.clearOutput ) |= clear;
    ImGui::PopStyleVar( 2 );
}

// Selected lines of one output tab. Indices into its messages, so a shorter list clears it.
struct MessageSelection {
    ImGuiSelectionBasicStorage lines;
    size_t messageCount = 0;
};

static MessageSelection consoleSelection;
static MessageSelection buildSelection;

static void copyMessages( const std::vector<Message> &messages,
                          const ImGuiSelectionBasicStorage *selection ) {
    std::string text;
    for ( size_t i = 0; i < messages.size(); i++ ) {
        if ( !selection || selection->Contains( static_cast<ImGuiID>( i ) ) ) {
            text += messages[i].text + "\n";
        }
    }
    ImGui::SetClipboardText( text.c_str() );
}

static void drawCopyMenu( const std::vector<Message> &messages,
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

// Lines are selected like a list: click, Shift/Ctrl+click, drag or Ctrl+A.
static void drawMessages( const std::vector<Message> &messages, MessageSelection &selection,
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
                jumpToSourceLine( message.sourceLine );
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

void drawConsolePanel( const Session &session, MenuActions &actions ) {
    followLatestBuild( session );
    beginPanel( "Console" );
    drawConsoleHeader( session, actions );
    switch ( shownOutputTab ) {
    case OutputTab::Console:
        drawMessages( session.console, consoleSelection );
        break;
    case OutputTab::Build:
        drawMessages( session.buildLog, buildSelection );
        break;
    case OutputTab::Terminal:
        drawTerminalView( session, actions );
        break;
    }
    ImGui::End();
}
