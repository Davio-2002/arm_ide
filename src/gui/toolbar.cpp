#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

#include <app/session.h>
#include <gui/commands.h>
#include <gui/icons.h>
#include <gui/theme.h>
#include <gui/toolbar.h>
#include <gui/widgets.h>

namespace {

const int buttonCount = 4;
const float buttonSpacing = 2.0f;
const float dropdownPadding = 10.0f;
const float chevronSize = 8.0f;
const float badgeSize = 26.0f;
const float badgeFontSize = 11.0f;
const float toolbarRightMargin = 32.0f;

/// @brief Draws a down arrow.
/// @param[in] center Centre of the arrow in screen coordinates.
/// @param[in] color  Line colour.
void drawChevronDown( ImVec2 center, ImU32 color ) {
    float half = chevronSize * 0.5f;
    ImVec2 points[] = { ImVec2( center.x - half, center.y - half * 0.5f ),
                        ImVec2( center.x, center.y + half * 0.5f ),
                        ImVec2( center.x + half, center.y - half * 0.5f ) };
    ImGui::GetWindowDrawList()->AddPolyline( points, 3, color, ImDrawFlags_None, 1.5f );
}

/// @param[in] label Dropdown label.
/// @return Width of a dropdown with this label, arrow included.
float dropdownWidth( const std::string &label ) {
    return ImGui::CalcTextSize( label.c_str() ).x + chevronSize + dropdownPadding * 3.0f;
}

/// @brief Draws a label with a down arrow that opens a popup below it.
/// @param[in] id         ImGui ID of the button and the popup.
/// @param[in] label      Text shown.
/// @param[in] background Fill when not hovered.
/// @return True while the popup is open; then the caller draws its items and calls
///         ImGui::EndPopup.
bool beginDropdown( const char *id, const std::string &label, ImU32 background ) {
    ImVec2 size( dropdownWidth( label ), ImGui::GetFrameHeight() );
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( id, size ) ) {
        ImGui::OpenPopup( id );
    }

    bool highlighted = ImGui::IsItemHovered() || ImGui::IsPopupOpen( id );
    ImU32 fill = highlighted ? accentFillColor() : background;
    ImVec2 bottomRight( topLeft.x + size.x, topLeft.y + size.y );
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled( topLeft, bottomRight, fill, ImGui::GetStyle().FrameRounding );
    outlineLastItem( highlighted );
    float textY = topLeft.y + ( size.y - ImGui::GetTextLineHeight() ) * 0.5f;
    drawList->AddText( ImVec2( topLeft.x + dropdownPadding, textY ), currentScheme().text,
                       label.c_str() );
    ImVec2 chevronCenter( bottomRight.x - dropdownPadding - chevronSize * 0.5f,
                          topLeft.y + size.y * 0.5f );
    drawChevronDown( chevronCenter, currentScheme().mutedText );

    ImGui::SetNextWindowPos( ImVec2( topLeft.x, bottomRight.y + 4.0f ) );
    return ImGui::BeginPopup( id );
}

/// @brief Draws the "A64" badge in the accent colour.
void drawAppBadge() {
    float margin = ( ImGui::GetFrameHeight() - badgeSize ) * 0.5f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 topLeft( cursor.x, cursor.y + margin );
    ImVec2 bottomRight( topLeft.x + badgeSize, topLeft.y + badgeSize );
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled( topLeft, bottomRight, accentColor(), 6.0f );

    ImGui::PushFont( nullptr, badgeFontSize );
    const char *name = "A64";
    ImVec2 textSize = ImGui::CalcTextSize( name );
    ImVec2 textPosition( topLeft.x + ( badgeSize - textSize.x ) * 0.5f,
                         topLeft.y + ( badgeSize - textSize.y ) * 0.5f );
    drawList->AddText( textPosition, IM_COL32_WHITE, name );
    ImGui::PopFont();
    ImGui::Dummy( ImVec2( badgeSize, ImGui::GetFrameHeight() ) );
}

/// @param[in] path File path; may be empty.
/// @return Name of the folder holding the file, or "No folder".
std::string folderName( const std::string &path ) {
    if ( path.empty() ) {
        return "No folder";
    }
    return std::filesystem::path( path ).parent_path().filename().string();
}

/// @brief Draws a file as a selectable name, with the full path as tooltip.
/// @param[in] path Absolute file path.
/// @return True if clicked.
bool fileItem( const std::string &path ) {
    ImGui::PushID( path.c_str() );
    bool clicked = ImGui::Selectable( documentName( path ).c_str() );
    ImGui::SetItemTooltip( "%s", displayPath( path ).c_str() );
    ImGui::PopID();
    return clicked;
}

} // anonymous namespace

void drawToolbarStart( const Session &session ) {
    drawAppBadge();
    std::string folder = folderName( session.activeDocument().path );
    if ( beginDropdown( "##recentFiles", folder, transparentColor ) ) {
        if ( session.recentFiles.empty() ) {
            ImGui::TextDisabled( "No recent files" );
        }
        for ( const std::string &path : session.recentFiles ) {
            if ( fileItem( path ) ) {
                requestAction( [path]( Session &session ) { session.openFile( path ); } );
            }
        }
        ImGui::EndPopup();
    }
    drawVerticalSeparator();
}

void drawToolbarEnd( const Session &session ) {
    float padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    float buttonWidth = iconSize + padding * 2.0f;
    float buttonsWidth = buttonCount * buttonWidth + ( buttonCount - 1 ) * buttonSpacing;
    std::string runFile = documentName( session.activeDocument().path );
    float toolbarWidth = dropdownWidth( runFile ) + buttonSpacing * 4.0f + buttonsWidth;
    ImGui::SetCursorPosX( ImGui::GetWindowWidth() - toolbarWidth - toolbarRightMargin );

    if ( beginDropdown( "##openFiles", runFile, currentScheme().background ) ) {
        for ( const Document &document : session.documents() ) {
            int id = document.id;
            if ( !document.path.empty() && fileItem( document.path ) ) {
                requestAction( [id]( Session &session ) { session.activateDocument( id ); } );
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine( 0.0f, buttonSpacing * 4.0f );

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( padding, padding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( buttonSpacing, 0.0f ) );
    commandButton( session, "##build", Command::Build, Icon::Build );
    commandButton( session, "##run", Command::Run, Icon::Run );
    commandButton( session, "##debug", Command::DebugContinue, Icon::Debug );
    commandButton( session, "##stop", Command::Stop, Icon::Stop );
    ImGui::PopStyleVar( 2 );
}
