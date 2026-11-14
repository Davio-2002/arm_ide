#include <imgui.h>

#include <filesystem>
#include <utility>
#include <vector>

#include <app/session.h>
#include <gui/commands.h>
#include <gui/dialogs.h>
#include <gui/panels/editor.h>
#include <gui/widgets.h>

namespace {

/// @brief A command's keyboard shortcut.
struct Shortcut {
    Command command;
    ImGuiKeyChord chord;
    const char *text;
};

/// The shortcut of each command that has one. A new command with a shortcut adds a row.
const Shortcut shortcuts[] = {
    { Command::NewFile, ImGuiMod_Ctrl | ImGuiKey_N, "Ctrl+N" },
    { Command::Open, ImGuiMod_Ctrl | ImGuiKey_O, "Ctrl+O" },
    { Command::Save, ImGuiMod_Ctrl | ImGuiKey_S, "Ctrl+S" },
    { Command::SaveAs, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, "Ctrl+Shift+S" },
    { Command::Build, ImGuiKey_F7, "F7" },
    { Command::Run, ImGuiKey_F5, "F5" },
    { Command::Stop, ImGuiMod_Shift | ImGuiKey_F5, "Shift+F5" },
    { Command::DebugContinue, ImGuiKey_F8, "F8" },
    { Command::DebugStep, ImGuiKey_F10, "F10" },
    { Command::ToggleBreakpoint, ImGuiKey_F9, "F9" },
};

std::vector<std::function<void( Session & )>> requests;

/// @brief Runs a command with the active editor's text, asking first for the file it needs.
/// @param[in,out] session Session to run it in.
void runCommand( Session &session, Command command ) {
    CommandInput input = editorCommandInput( session );
    auto runWithPath = [&session, command, input]( const std::string &path ) mutable {
        input.path = path;
        session.run( command, input );
    };
    const std::string &activePath = session.activeDocument().path;
    switch ( session.pathNeeded( command ) ) {
    case PathRequest::None:
        session.run( command, input );
        break;
    case PathRequest::FileToOpen:
        askForFileToOpen( std::filesystem::path( activePath ).parent_path(), runWithPath );
        break;
    case PathRequest::FileToSave:
        askForFileToSave( activePath.empty() ? "untitled.s" : activePath, runWithPath );
        break;
    }
}

} // anonymous namespace

void requestCommand( Command command ) {
    requests.push_back( [command]( Session &session ) { runCommand( session, command ); } );
}

void requestAction( std::function<void( Session & )> action ) {
    requests.push_back( std::move( action ) );
}

void runRequests( Session &session ) {
    for ( const auto &request : std::exchange( requests, {} ) ) {
        if ( !isDialogOpen() ) {
            request( session );
        }
    }
}

void readCommandShortcuts() {
    for ( const Shortcut &shortcut : shortcuts ) {
        if ( ImGui::Shortcut( shortcut.chord, ImGuiInputFlags_RouteGlobal ) ) {
            requestCommand( shortcut.command );
        }
    }
}

const char *commandShortcut( Command command ) {
    for ( const Shortcut &shortcut : shortcuts ) {
        if ( shortcut.command == command ) {
            return shortcut.text;
        }
    }
    return "";
}

void commandMenuItem( const Session &session, Command command ) {
    std::string label = session.commandLabel( command );
    if ( ImGui::MenuItem( label.c_str(), commandShortcut( command ), false,
                          session.canRun( command ) ) ) {
        requestCommand( command );
    }
}

void commandButton( const Session &session, const char *id, Command command, Icon icon ) {
    std::string tooltip = session.commandLabel( command ) + "  " + commandShortcut( command );
    if ( iconButton( id, icon, tooltip.c_str(), session.canRun( command ) ) ) {
        requestCommand( command );
    }
}

void whenChangesDiscarded( const Session &session, std::function<void()> action ) {
    std::vector<std::string> unsaved = session.unsavedDocumentNames();
    if ( unsaved.empty() ) {
        action();
        return;
    }
    std::string what =
        unsaved.size() == 1 ? unsaved.front() : std::to_string( unsaved.size() ) + " files";
    askToDiscardChanges( what, std::move( action ) );
}
