#include <imgui.h>

#include <functional>

#include <app/session.h>
#include <gui/appearance.h>
#include <gui/commands.h>
#include <gui/fonts.h>
#include <gui/menu.h>
#include <gui/theme.h>
#include <gui/toolbar.h>
#include <gui/widgets.h>

namespace {

/// @brief Reads the zoom shortcuts.
void readViewShortcuts() {
    ImGuiInputFlags global = ImGuiInputFlags_RouteGlobal;
    if ( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Equal, global ) ||
         ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal, global ) ||
         ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_KeypadAdd, global ) ) {
        zoomCode( 2.0f );
    }
    if ( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Minus, global ) ||
         ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract, global ) ) {
        zoomCode( -2.0f );
    }
    if ( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_0, global ) ||
         ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Keypad0, global ) ) {
        resetZoom();
    }
}

/// @brief Accent-lit, rounded menu items; pop with popMenuStyle().
void pushMenuStyle() {
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding,
                         ImVec2( ImGui::GetStyle().FramePadding.x, toolbarFramePaddingY ) );
    ImGui::PushStyleVar( ImGuiStyleVar_SelectableRounding, ImGui::GetStyle().FrameRounding );
    for ( ImGuiCol color : { ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive } ) {
        ImGui::PushStyleColor( color, accentFillColor() );
    }
}

void popMenuStyle() {
    ImGui::PopStyleColor( 3 );
    ImGui::PopStyleVar( 2 );
}

/// @brief A menu outlined while hovered or open.
void drawMenu( const char *label, const std::function<void()> &drawItems ) {
    bool open = ImGui::BeginMenu( label );
    if ( open ) {
        drawItems();
        ImGui::EndMenu();
    }
    outlineLastItem( open || ImGui::IsItemHovered() );
}

void drawFileMenu( const Session &session ) {
    commandMenuItem( session, Command::NewFile );
    commandMenuItem( session, Command::Open );
    if ( ImGui::BeginMenu( "Open Recent", !session.recentFiles.empty() ) ) {
        for ( const std::string &path : session.recentFiles ) {
            if ( ImGui::MenuItem( displayPath( path ).c_str() ) ) {
                requestAction( [path]( Session &session ) { session.openFile( path ); } );
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    commandMenuItem( session, Command::Save );
    commandMenuItem( session, Command::SaveAs );
}

void drawViewMenu() {
    if ( ImGui::MenuItem( "Zoom In", "Ctrl+=" ) ) {
        zoomCode( 2.0f );
    }
    if ( ImGui::MenuItem( "Zoom Out", "Ctrl+-" ) ) {
        zoomCode( -2.0f );
    }
    if ( ImGui::MenuItem( "Reset Zoom", "Ctrl+0" ) ) {
        resetZoom();
    }
    ImGui::Separator();
    if ( ImGui::MenuItem( "Appearance..." ) ) {
        openAppearance();
    }
}

} // anonymous namespace

void drawMenuBar( const Session &session ) {
    readCommandShortcuts();
    readViewShortcuts();

    pushMenuStyle();
    if ( !ImGui::BeginMainMenuBar() ) {
        popMenuStyle();
        return;
    }
    drawToolbarStart( session );
    drawMenu( "File", [&session] { drawFileMenu( session ); } );
    drawMenu( "View", drawViewMenu );
    drawMenu( "Build", [&session] { commandMenuItem( session, Command::Build ); } );
    drawMenu( "Run", [&session] {
        commandMenuItem( session, Command::Run );
        commandMenuItem( session, Command::Stop );
    } );
    drawMenu( "Debug", [&session] {
        commandMenuItem( session, Command::DebugContinue );
        commandMenuItem( session, Command::DebugStep );
        commandMenuItem( session, Command::ToggleBreakpoint );
    } );
    drawToolbarEnd( session );
    ImGui::EndMainMenuBar();
    popMenuStyle();
}
