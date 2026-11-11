#include <imgui.h>

#include <gui/icons.h>
#include <gui/theme.h>
#include <gui/widgets.h>

static const float tabPadding = 10.0f;
static const float tabIconGap = 6.0f;
static const float closeSize = 16.0f;

static bool drawIconButton( const char *id, Icon icon, const char *tooltip, bool enabled,
                            ImU32 background ) {
    ImGui::PushStyleColor( ImGuiCol_Button, background );
    ImGui::PushStyleColor( ImGuiCol_ButtonHovered, selectedColor );
    ImGui::PushStyleColor( ImGuiCol_ButtonActive, borderColor );
    ImGui::BeginDisabled( !enabled );
    bool clicked = ImGui::ImageButton( id, iconTexture( icon ), ImVec2( iconSize, iconSize ) );
    ImGui::SetItemTooltip( "%s", tooltip );
    ImGui::EndDisabled();
    ImGui::PopStyleColor( 3 );
    return clicked;
}

bool iconButton( const char *id, Icon icon, const char *tooltip, bool enabled ) {
    return drawIconButton( id, icon, tooltip, enabled, transparentColor );
}

void iconToggle( const char *id, Icon icon, const char *tooltip, bool &on ) {
    if ( drawIconButton( id, icon, tooltip, true, on ? selectedColor : transparentColor ) ) {
        on = !on;
    }
}

void drawVerticalSeparator() {
    float height = ImGui::GetFrameHeight();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float x = cursor.x + 6.0f;
    ImGui::GetWindowDrawList()->AddLine( ImVec2( x, cursor.y + height * 0.25f ),
                                         ImVec2( x, cursor.y + height * 0.75f ), borderColor );
    ImGui::Dummy( ImVec2( 12.0f, height ) );
}

static float pillTabWidth( const PillTab &tab ) {
    float width = tabPadding * 2.0f + ImGui::CalcTextSize( tab.label.c_str() ).x;
    if ( tab.hasIcon ) {
        width += iconSize + tabIconGap;
    }
    if ( tab.closable ) {
        width += tabIconGap + closeSize;
    }
    return width;
}

static void drawCross( ImVec2 center, ImU32 color ) {
    float half = closeSize * 0.22f;
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddLine( ImVec2( center.x - half, center.y - half ),
                       ImVec2( center.x + half, center.y + half ), color, 1.5f );
    drawList->AddLine( ImVec2( center.x - half, center.y + half ),
                       ImVec2( center.x + half, center.y - half ), color, 1.5f );
}

TabClick drawPillTab( const char *id, const PillTab &tab ) {
    ImVec2 size( pillTabWidth( tab ), ImGui::GetFrameHeight() );
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton( id, size );
    bool hovered = ImGui::IsItemHovered();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 bottomRight( topLeft.x + size.x, topLeft.y + size.y );
    if ( tab.selected || hovered ) {
        ImU32 fill = tab.selected ? selectedColor : hoveredColor;
        drawList->AddRectFilled( topLeft, bottomRight, fill, ImGui::GetStyle().FrameRounding );
    }

    float x = topLeft.x + tabPadding;
    float centerY = topLeft.y + size.y * 0.5f;
    if ( tab.hasIcon ) {
        ImVec2 iconTopLeft( x, centerY - iconSize * 0.5f );
        ImVec2 iconBottomRight( x + iconSize, centerY + iconSize * 0.5f );
        drawList->AddImage( iconTexture( tab.icon ), iconTopLeft, iconBottomRight );
        x += iconSize + tabIconGap;
    }
    ImU32 labelColor = tab.selected ? textColor : secondaryTextColor;
    drawList->AddText( ImVec2( x, centerY - ImGui::GetTextLineHeight() * 0.5f ), labelColor,
                       tab.label.c_str() );
    if ( !tab.closable ) {
        return clicked ? TabClick::Select : TabClick::None;
    }

    ImVec2 closeCenter( bottomRight.x - tabPadding - closeSize * 0.5f, centerY );
    bool overClose = hovered && ImGui::GetMousePos().x >= closeCenter.x - closeSize * 0.5f;
    if ( tab.unsaved && !hovered ) {
        drawList->AddCircleFilled( closeCenter, 4.0f, pausedColor );
    } else if ( tab.selected || hovered ) {
        if ( overClose ) {
            drawList->AddRectFilled(
                ImVec2( closeCenter.x - closeSize * 0.5f, centerY - closeSize * 0.5f ),
                ImVec2( closeCenter.x + closeSize * 0.5f, centerY + closeSize * 0.5f ), borderColor,
                4.0f );
        }
        drawCross( closeCenter, overClose ? textColor : mutedTextColor );
    }

    if ( !clicked ) {
        return TabClick::None;
    }
    return overClose ? TabClick::Close : TabClick::Select;
}
