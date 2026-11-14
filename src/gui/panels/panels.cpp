#include <imgui.h>
#include <imgui_internal.h>

#include <iterator>

#include <app/session.h>
#include <gui/icons.h>
#include <gui/panels/editor.h>
#include <gui/panels/panels.h>
#include <gui/theme.h>
#include <gui/widgets.h>

namespace {

const float toolStripWidth = 44.0f;

/// @brief Where a panel is docked in the default layout.
enum class DockArea { Main, Bottom, Right, RightBottom };

/// @brief One dock panel.
struct Panel {
    const char *name;
    DockArea area;
    /// True to show a button on the tool strip that shows and hides the panel.
    bool canHide;
    Icon icon;
    bool visible;
    void ( *draw )( Session &session );
};

/// Every panel, drawn in this order. A new panel adds a row here.
Panel panels[] = {
    { "Editor", DockArea::Main, false, Icon::AsmFile, true,
      []( Session &session ) { drawEditorPanel( session ); } },
    { "Console", DockArea::Bottom, false, Icon::Console, true,
      []( Session &session ) { drawConsolePanel( session ); } },
    { "Registers", DockArea::Right, true, Icon::Registers, true,
      []( Session &session ) { drawRegistersPanel( session ); } },
    { "Memory", DockArea::RightBottom, true, Icon::Memory, true,
      []( Session &session ) { drawMemoryPanel( session ); } },
};

/// @brief Docks the panels in the default layout: the main area with the bottom one below it
///        on the left, the right one with the right-bottom one below it on the right.
/// @param[in] dockspace Dock space to build.
void createDefaultLayout( ImGuiID dockspace ) {
    ImGui::DockBuilderAddNode( dockspace, ImGuiDockNodeFlags_DockSpace );
    ImGui::DockBuilderSetNodeSize( dockspace, ImGui::GetMainViewport()->WorkSize );

    ImGuiID leftNode, rightNode;
    ImGui::DockBuilderSplitNode( dockspace, ImGuiDir_Right, 0.3f, &rightNode, &leftNode );
    ImGuiID mainNode, bottomNode;
    ImGui::DockBuilderSplitNode( leftNode, ImGuiDir_Down, 0.25f, &bottomNode, &mainNode );
    ImGuiID rightTopNode, rightBottomNode;
    ImGui::DockBuilderSplitNode( rightNode, ImGuiDir_Down, 0.4f, &rightBottomNode, &rightTopNode );

    ImGuiID nodes[] = { mainNode, bottomNode, rightTopNode, rightBottomNode };
    for ( const Panel &panel : panels ) {
        ImGui::DockBuilderDockWindow( panel.name, nodes[static_cast<int>( panel.area )] );
    }
    ImGui::DockBuilderFinish( dockspace );
}

/// @brief Draws the dock space over the whole work area, creating the default layout once.
///
/// The ground window and dock title bars stay transparent, and each docked panel draws its
/// rounded background itself.
void drawDockSpace() {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( viewport->WorkPos );
    ImGui::SetNextWindowSize( viewport->WorkSize );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( panelGap, panelGap ) );
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

/// @brief Draws the bar on the right edge whose buttons show and hide the side panels.
void drawToolStrip() {
    float buttonPadding = 6.0f;
    float sidePadding = ( toolStripWidth - iconSize ) * 0.5f - buttonPadding;
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( sidePadding, panelGap ) );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( buttonPadding, buttonPadding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 4.0f ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, currentScheme().ground );
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    if ( ImGui::BeginViewportSideBar( "##ToolStrip", ImGui::GetMainViewport(), ImGuiDir_Right,
                                      toolStripWidth, flags ) ) {
        for ( Panel &panel : panels ) {
            if ( panel.canHide ) {
                ImGui::PushID( panel.name );
                iconToggle( "##toggle", panel.icon, panel.name, panel.visible );
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar( 3 );
}

} // anonymous namespace

void beginPanel( const char *name, ImGuiWindowFlags flags ) {
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
        ImVec2 panelEnd( node->Pos.x + node->Size.x, node->Pos.y + node->Size.y );
        ImDrawList *drawList = ImGui::GetBackgroundDrawList();
        drawList->AddRectFilled( node->Pos, panelEnd, currentScheme().background, panelRounding );
        drawList->AddRect( node->Pos, panelEnd, accentColor(), panelRounding );
    }
}

void drawPanelTitle( const char *title ) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( title );
}

void pushHeaderButtonPadding() {
    float padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( padding, padding ) );
}

float headerButtonWidth() {
    return ImGui::GetFrameHeight();
}

void drawPanels( Session &session ) {
    syncEditorWithSession( session );
    drawToolStrip();
    drawDockSpace();
    for ( const Panel &panel : panels ) {
        if ( panel.visible ) {
            panel.draw( session );
        }
    }
}
