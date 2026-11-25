#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <iterator>

#include <app/session.h>
#include <gui/icons.h>
#include <gui/panels/editor.h>
#include <gui/panels/panels.h>
#include <gui/theme.h>
#include <gui/widgets.h>

namespace {

const float toolStripWidth = 44.0f;

/// @brief Dock area in the default layout.
enum class DockArea { Main, Bottom, Right, RightBottom };

struct Panel {
    const char *name;
    DockArea area;
    bool canHide;
    Icon icon;
    bool visible;
    void ( *draw )( Session &session, TerminalView &terminalView );
};

/// Drawn in this order; a new panel adds a row.
auto panels = std::to_array<Panel>( {
    { "Editor", DockArea::Main, false, Icon::AsmFile, true,
      []( Session &session, TerminalView & ) { drawEditorPanel( session ); } },
    { "Console", DockArea::Bottom, false, Icon::Console, true,
      []( Session &session, TerminalView &terminalView ) {
          drawConsolePanel( session, terminalView );
      } },
    { "Registers", DockArea::Right, true, Icon::Registers, true,
      []( Session &session, TerminalView & ) { drawRegistersPanel( session ); } },
    { "Memory", DockArea::RightBottom, true, Icon::Memory, true,
      []( Session &session, TerminalView & ) { drawMemoryPanel( session ); } },
} );

/// @brief Main and bottom on the left, right and right-bottom on the right.
void createDefaultLayout( ImGuiID dockspace ) {
    ImGui::DockBuilderAddNode( dockspace, ImGuiDockNodeFlags_DockSpace );
    ImGui::DockBuilderSetNodeSize( dockspace, ImGui::GetMainViewport()->WorkSize );

    ImGuiID leftNode, rightNode;
    ImGui::DockBuilderSplitNode( dockspace, ImGuiDir_Right, 0.3f, &rightNode, &leftNode );
    ImGuiID mainNode, bottomNode;
    ImGui::DockBuilderSplitNode( leftNode, ImGuiDir_Down, 0.25f, &bottomNode, &mainNode );
    ImGuiID rightTopNode, rightBottomNode;
    ImGui::DockBuilderSplitNode( rightNode, ImGuiDir_Down, 0.4f, &rightBottomNode, &rightTopNode );

    std::array const nodes = { mainNode, bottomNode, rightTopNode, rightBottomNode };
    for ( const Panel &panel : panels ) {
        ImGui::DockBuilderDockWindow( panel.name, nodes[static_cast<int>( panel.area )] );
    }
    ImGui::DockBuilderFinish( dockspace );
}

/// @brief Dock space over the work area, default layout created once. Ground and title bars
///        stay transparent; each docked panel draws its rounded background.
void drawDockSpace() {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( viewport->WorkPos );
    ImGui::SetNextWindowSize( viewport->WorkSize );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( panelGap, panelGap ) );
    ImGuiWindowFlags const groundFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin( "##Ground", nullptr, groundFlags );
    ImGui::PopStyleVar();

    ImGuiID const dockspace = ImGui::GetID( "MainDockSpace v3" );
    if ( !ImGui::DockBuilderGetNode( dockspace ) ) {
        createDefaultLayout( dockspace );
    }
    auto const hiddenColors = std::to_array<ImGuiCol>(
        { ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_TitleBgCollapsed, ImGuiCol_Border,
          ImGuiCol_DockingEmptyBg, ImGuiCol_WindowBg } );
    for ( ImGuiCol const color : hiddenColors ) {
        ImGui::PushStyleColor( color, transparentColor );
    }
    ImGui::DockSpace( dockspace );
    ImGui::PopStyleColor( std::size( hiddenColors ) );
    ImGui::End();
}

/// @brief Right-edge bar whose buttons show and hide the side panels.
void drawToolStrip() {
    float const buttonPadding = 6.0f;
    float const sidePadding = ( toolStripWidth - iconSize ) * 0.5f - buttonPadding;
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( sidePadding, panelGap ) );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( buttonPadding, buttonPadding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 4.0f ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, currentScheme().ground );
    ImGuiWindowFlags const flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
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

    ImGuiWindow const *window = ImGui::FindWindowByName( name );
    bool const docked = !window || window->DockNode != nullptr;
    if ( docked ) {
        ImGui::PushStyleColor( ImGuiCol_WindowBg, transparentColor );
    }
    ImGui::Begin( name, nullptr, flags );
    if ( docked ) {
        ImGui::PopStyleColor();
    }

    if ( ImGuiDockNode const *node = ImGui::GetWindowDockNode() ) {
        ImVec2 const panelEnd( node->Pos.x + node->Size.x, node->Pos.y + node->Size.y );
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
    float const padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( padding, padding ) );
}

float headerButtonWidth() {
    return ImGui::GetFrameHeight();
}

void drawPanels( Session &session, TerminalView &terminalView ) {
    syncEditorWithSession( session );
    drawToolStrip();
    drawDockSpace();
    for ( const Panel &panel : panels ) {
        if ( panel.visible ) {
            panel.draw( session, terminalView );
        }
    }
}
