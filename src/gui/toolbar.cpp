#include <imgui.h>

#include <app/session.h>
#include <gui/icons.h>
#include <gui/menu.h>
#include <gui/theme.h>
#include <gui/toolbar.h>

static const int buttonCount = 4;
static const float buttonSpacing = 2.0f;

bool iconButton( const char *id, Icon icon, const char *tooltip, bool enabled ) {
    ImGui::PushStyleColor( ImGuiCol_Button, transparentColor );
    ImGui::PushStyleColor( ImGuiCol_ButtonHovered, selectedColor );
    ImGui::PushStyleColor( ImGuiCol_ButtonActive, borderColor );
    ImGui::BeginDisabled( !enabled );
    bool clicked = ImGui::ImageButton( id, iconTexture( icon ), ImVec2( iconSize, iconSize ) );
    ImGui::SetItemTooltip( "%s", tooltip );
    ImGui::EndDisabled();
    ImGui::PopStyleColor( 3 );
    return clicked;
}

void drawToolbar( const Session &session, MenuActions &actions ) {
    float padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    float buttonWidth = iconSize + padding * 2.0f;
    float toolbarWidth = buttonCount * buttonWidth + ( buttonCount - 1 ) * buttonSpacing;
    ImGui::SetCursorPosX( ImGui::GetWindowWidth() - toolbarWidth -
                          ImGui::GetStyle().WindowPadding.x );

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( padding, padding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( buttonSpacing, 0.0f ) );

    actions.build |= iconButton( "##build", Icon::Build, "Build  F7" );
    actions.run |= iconButton( "##run", Icon::Run, "Build and Run  F5" );
    actions.debugContinue |= iconButton( "##debug", Icon::Debug, "Debug  F8" );
    actions.stop |= iconButton( "##stop", Icon::Stop, "Stop  Shift+F5", session.isProgramActive() );

    ImGui::PopStyleVar( 2 );
}
