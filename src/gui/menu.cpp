#include <imgui.h>
#include <imgui_internal.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>

#include <app/session.h>
#include <gui/dialogs.h>
#include <gui/menu.h>
#include <gui/panels/editor.h>
#include <gui/theme.h>
#include <gui/toolbar.h>
#include <gui/widgets.h>
#include <gui/window.h>

void openDocument( Session &session, const std::string &path ) {
    std::string text;
    if ( session.openFile( path, text ) ) {
        openInEditor( session.filePath, text );
    }
    ImGui::MarkIniSettingsDirty();
}

void whenChangesDiscarded( std::function<void()> action ) {
    std::vector<std::string> unsaved = unsavedDocumentNames();
    if ( unsaved.empty() ) {
        action();
        return;
    }
    std::string what =
        unsaved.size() == 1 ? unsaved.front() : std::to_string( unsaved.size() ) + " files";
    askToDiscardChanges( what, std::move( action ) );
}

static void saveDocument( Session &session, const std::string &path ) {
    if ( session.saveFile( path, editorText() ) ) {
        markEditorSaved( session.filePath );
    }
    ImGui::MarkIniSettingsDirty();
}

std::string displayPath( const std::string &path ) {
    const char *home = std::getenv( "HOME" );
    if ( home && path.starts_with( std::string( home ) + "/" ) ) {
        return "~" + path.substr( std::strlen( home ) );
    }
    return path;
}

static MenuActions readShortcuts() {
    ImGuiInputFlags global = ImGuiInputFlags_RouteGlobal;
    MenuActions actions;
    actions.open = ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_O, global );
    actions.save = ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_S, global );
    actions.saveAs = ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, global );
    actions.zoomIn = ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Equal, global ) ||
                     ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal, global ) ||
                     ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_KeypadAdd, global );
    actions.zoomOut = ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Minus, global ) ||
                      ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract, global );
    actions.resetZoom = ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_0, global ) ||
                        ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Keypad0, global );
    actions.build = ImGui::Shortcut( ImGuiKey_F7, global );
    actions.run = ImGui::Shortcut( ImGuiKey_F5, global );
    actions.stop = ImGui::Shortcut( ImGuiMod_Shift | ImGuiKey_F5, global );
    actions.debugContinue = ImGui::Shortcut( ImGuiKey_F8, global );
    actions.debugStep = ImGui::Shortcut( ImGuiKey_F10, global );
    actions.toggleBreakpoint = ImGui::Shortcut( ImGuiKey_F9, global );
    return actions;
}

// Menus and their items light up in the accent, rounded like the tabs.
static void pushMenuStyle() {
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding,
                         ImVec2( ImGui::GetStyle().FramePadding.x, toolbarFramePaddingY ) );
    ImGui::PushStyleVar( ImGuiStyleVar_SelectableRounding, ImGui::GetStyle().FrameRounding );
    for ( ImGuiCol color : { ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive } ) {
        ImGui::PushStyleColor( color, accentFillColor() );
    }
}

static void popMenuStyle() {
    ImGui::PopStyleColor( 3 );
    ImGui::PopStyleVar( 2 );
}

static void drawMenus( const Session &session, MenuActions &actions ) {
    pushMenuStyle();
    if ( !ImGui::BeginMainMenuBar() ) {
        popMenuStyle();
        return;
    }
    drawToolbarStart( session, actions );

    bool fileOpen = ImGui::BeginMenu( "File" );
    if ( fileOpen ) {
        actions.open |= ImGui::MenuItem( "Open...", "Ctrl+O" );
        if ( ImGui::BeginMenu( "Open Recent", !session.recentFiles.empty() ) ) {
            for ( const std::string &path : session.recentFiles ) {
                if ( ImGui::MenuItem( displayPath( path ).c_str() ) ) {
                    actions.fileToOpen = path;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        actions.save |= ImGui::MenuItem( "Save", "Ctrl+S" );
        actions.saveAs |= ImGui::MenuItem( "Save As...", "Ctrl+Shift+S" );
        ImGui::EndMenu();
    }
    outlineLastItem( fileOpen || ImGui::IsItemHovered() );
    bool viewOpen = ImGui::BeginMenu( "View" );
    if ( viewOpen ) {
        actions.zoomIn |= ImGui::MenuItem( "Zoom In", "Ctrl+=" );
        actions.zoomOut |= ImGui::MenuItem( "Zoom Out", "Ctrl+-" );
        actions.resetZoom |= ImGui::MenuItem( "Reset Zoom", "Ctrl+0" );
        ImGui::Separator();
        actions.openAccentColor |= ImGui::MenuItem( "Accent Color..." );
        actions.openPanelColor |= ImGui::MenuItem( "Panel Background..." );
        ImGui::EndMenu();
    }
    outlineLastItem( viewOpen || ImGui::IsItemHovered() );
    bool buildOpen = ImGui::BeginMenu( "Build" );
    if ( buildOpen ) {
        actions.build |= ImGui::MenuItem( "Build", "F7" );
        ImGui::EndMenu();
    }
    outlineLastItem( buildOpen || ImGui::IsItemHovered() );
    bool runOpen = ImGui::BeginMenu( "Run" );
    if ( runOpen ) {
        actions.run |= ImGui::MenuItem( "Build and Run", "F5" );
        actions.stop |= ImGui::MenuItem( "Stop", "Shift+F5", false, session.isProgramActive() );
        ImGui::EndMenu();
    }
    outlineLastItem( runOpen || ImGui::IsItemHovered() );
    bool debugOpen = ImGui::BeginMenu( "Debug" );
    if ( debugOpen ) {
        const char *continueLabel = session.isDebugging() ? "Continue" : "Start Debugging";
        actions.debugContinue |= ImGui::MenuItem( continueLabel, "F8" );
        actions.debugStep |= ImGui::MenuItem( "Step", "F10" );
        actions.toggleBreakpoint |= ImGui::MenuItem( "Toggle Breakpoint", "F9" );
        ImGui::EndMenu();
    }
    outlineLastItem( debugOpen || ImGui::IsItemHovered() );

    drawToolbarEnd( session, actions );
    ImGui::EndMainMenuBar();
    popMenuStyle();
}

static void runProgramActions( Session &session, const MenuActions &actions ) {
    if ( actions.run ) {
        session.run();
    } else if ( actions.debugContinue ) {
        session.debugContinue();
    } else if ( actions.debugStep ) {
        session.debugStep();
    } else if ( actions.build ) {
        session.build();
    }
}

static void runActions( Session &session, const MenuActions &actions ) {
    if ( actions.zoomIn ) {
        zoomCode( 2.0f );
    }
    if ( actions.zoomOut ) {
        zoomCode( -2.0f );
    }
    if ( actions.resetZoom ) {
        resetZoom();
    }
    if ( actions.openAccentColor ) {
        openAccentColorDialog();
    }
    if ( actions.openPanelColor ) {
        openPanelColorDialog();
    }

    if ( actions.stop ) {
        session.stop();
    }
    if ( actions.clearOutput ) {
        session.clearOutput();
    }
    if ( actions.sendProgramInput ) {
        session.sendProgramInput( actions.programInput );
    }
    if ( actions.endProgramInput ) {
        session.endProgramInput();
    }
    if ( actions.terminalRows > 0 && actions.terminalColumns > 0 ) {
        session.openTerminal( actions.terminalRows, actions.terminalColumns );
    }
    session.sendTerminalInput( actions.terminalInputs, actions.terminalPaste );
    if ( actions.stopTerminal ) {
        session.stopTerminal();
    }
    if ( actions.clearTerminal ) {
        session.clearTerminal();
    }
    if ( actions.toggleBreakpoint ) {
        session.toggleBreakpoint( session.filePath, editorCursorLine() );
    }
    if ( actions.open ) {
        std::string directory = std::filesystem::path( session.filePath ).parent_path();
        askForFileToOpen(
            directory, [&session]( const std::string &path ) { openDocument( session, path ); } );
    }
    if ( !actions.fileToOpen.empty() ) {
        openDocument( session, actions.fileToOpen );
    }

    bool startsDebugging = ( actions.debugContinue || actions.debugStep ) && !session.isDebugging();
    bool rebuilds = actions.build || actions.run || startsDebugging;
    bool needsPath = ( actions.save || rebuilds ) && session.filePath.empty();
    if ( actions.saveAs || needsPath ) {
        std::string suggestedPath = session.filePath.empty() ? "untitled.s" : session.filePath;
        askForFileToSave( suggestedPath, [&session, actions]( const std::string &path ) {
            saveDocument( session, path );
            runProgramActions( session, actions );
        } );
        return;
    }
    if ( actions.save || rebuilds ) {
        saveDocument( session, session.filePath );
    }
    runProgramActions( session, actions );
}

MenuActions drawMenuBar( const Session &session ) {
    MenuActions actions = readShortcuts();
    drawMenus( session, actions );
    return actions;
}

void runMenuActions( Session &session, const MenuActions &actions ) {
    if ( !isDialogOpen() ) {
        runActions( session, actions );
    }
}
