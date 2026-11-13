#include <imgui.h>
#include <imgui_internal.h>

#include <iterator>

#include <gui/panels/console.h>
#include <gui/panels/editor.h>
#include <gui/panels/memory.h>
#include <gui/panels/panels.h>
#include <gui/panels/registers.h>
#include <gui/theme.h>
#include <gui/widgets.h>

static const float toolStripWidth = 44.0f;

static bool showRegisters = true;
static bool showMemory = true;

/// @brief Docks the panels in the default layout: editor with the console below it on the
///        left, registers with memory below them on the right.
/// @param[in] dockspace Dock space to build.
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

/// @brief Draws the dock space over the whole work area, creating the default layout once.
///
/// The ground window and dock title bars stay transparent, and each docked panel draws its
/// rounded island background itself.
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
        ImVec2 islandEnd( node->Pos.x + node->Size.x, node->Pos.y + node->Size.y );
        ImDrawList *drawList = ImGui::GetBackgroundDrawList();
        drawList->AddRectFilled( node->Pos, islandEnd, islandColor(), islandRounding );
        drawList->AddRect( node->Pos, islandEnd, accentColor(), islandRounding );
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

/// @brief Draws the bar on the right edge whose buttons show and hide the side panels.
static void drawToolStrip() {
    float buttonPadding = 6.0f;
    float sidePadding = ( toolStripWidth - iconSize ) * 0.5f - buttonPadding;
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( sidePadding, islandGap ) );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( buttonPadding, buttonPadding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 4.0f ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, groundColor() );
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    if ( ImGui::BeginViewportSideBar( "##ToolStrip", ImGui::GetMainViewport(), ImGuiDir_Right,
                                      toolStripWidth, flags ) ) {
        iconToggle( "##registers", Icon::Registers, "Registers", showRegisters );
        iconToggle( "##memory", Icon::Memory, "Memory", showMemory );
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar( 3 );
}

void drawPanels( Session &session, MenuActions &actions ) {
    syncEditorWithSession( session );
    drawToolStrip();
    drawDockSpace();
    drawEditorPanel();
    drawConsolePanel( session, actions );
    if ( showRegisters ) {
        drawRegistersPanel( session );
    }
    if ( showMemory ) {
        drawMemoryPanel( session );
    }
}
