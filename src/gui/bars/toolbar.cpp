#include <imgui.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <app/session/session.h>
#include <gui/bars/toolbar.h>
#include <gui/look/icons.h>
#include <gui/look/theme.h>
#include <gui/look/widgets.h>
#include <gui/window/commands.h>

namespace {

const int buttonCount = 4;
const float buttonSpacing = 2.0f;
const float dropdownPadding = 10.0f;
const float chevronSize = 8.0f;
const float badgeSize = 26.0f;
const float badgeFontSize = 11.0f;
const float toolbarRightMargin = 32.0f;

void drawChevronDown( ImVec2 center, ImU32 color ) {
    float const half = chevronSize * 0.5f;
    std::array const points = { ImVec2( center.x - half, center.y - half * 0.5f ),
                                ImVec2( center.x, center.y + half * 0.5f ),
                                ImVec2( center.x + half, center.y - half * 0.5f ) };
    ImGui::GetWindowDrawList()->AddPolyline( points.data(), points.size(), color, ImDrawFlags_None,
                                             1.5f );
}

/// @return Dropdown width, arrow included.
float dropdownWidth( const std::string &label ) {
    return ImGui::CalcTextSize( label.c_str() ).x + chevronSize + dropdownPadding * 3.0f;
}

/// @brief A label with a down arrow that opens a popup below it.
/// @param[in] background Fill when not hovered.
/// @return True while open; the caller then draws items and calls ImGui::EndPopup.
bool beginDropdown( const char *id, const std::string &label, ImU32 background ) {
    ImVec2 const size( dropdownWidth( label ), ImGui::GetFrameHeight() );
    ImVec2 const topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( id, size ) ) {
        ImGui::OpenPopup( id );
    }

    bool const highlighted = ImGui::IsItemHovered() || ImGui::IsPopupOpen( id );
    ImU32 const fill = highlighted ? accentFillColor() : background;
    ImVec2 const bottomRight( topLeft.x + size.x, topLeft.y + size.y );
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled( topLeft, bottomRight, fill, ImGui::GetStyle().FrameRounding );
    outlineLastItem( highlighted );
    float const textY = topLeft.y + ( size.y - ImGui::GetTextLineHeight() ) * 0.5f;
    drawList->AddText( ImVec2( topLeft.x + dropdownPadding, textY ), currentScheme().text,
                       label.c_str() );
    ImVec2 const chevronCenter( bottomRight.x - dropdownPadding - chevronSize * 0.5f,
                                topLeft.y + size.y * 0.5f );
    drawChevronDown( chevronCenter, currentScheme().mutedText );

    ImGui::SetNextWindowPos( ImVec2( topLeft.x, bottomRight.y + 4.0f ) );
    return ImGui::BeginPopup( id );
}

void drawAppBadge() {
    float const margin = ( ImGui::GetFrameHeight() - badgeSize ) * 0.5f;
    ImVec2 const cursor = ImGui::GetCursorScreenPos();
    ImVec2 const topLeft( cursor.x, cursor.y + margin );
    ImVec2 const bottomRight( topLeft.x + badgeSize, topLeft.y + badgeSize );
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled( topLeft, bottomRight, accentColor(), 6.0f );

    ImGui::PushFont( nullptr, badgeFontSize );
    const char *name = "A64";
    ImVec2 const textSize = ImGui::CalcTextSize( name );
    ImVec2 const textPosition( topLeft.x + ( badgeSize - textSize.x ) * 0.5f,
                               topLeft.y + ( badgeSize - textSize.y ) * 0.5f );
    drawList->AddText( textPosition, IM_COL32_WHITE, name );
    ImGui::PopFont();
    ImGui::Dummy( ImVec2( badgeSize, ImGui::GetFrameHeight() ) );
}

/// @return Folder of the file, or "No folder".
std::string folderName( const std::string &path ) {
    if ( path.empty() ) {
        return "No folder";
    }
    return std::filesystem::path( path ).parent_path().filename().string();
}

/// @brief File name with the full path as tooltip.
/// @return True if clicked.
bool fileItem( const std::string &path ) {
    ImGui::PushID( path.c_str() );
    bool const clicked = ImGui::Selectable( documentName( path ).c_str() );
    ImGui::SetItemTooltip( "%s", displayPath( path ).c_str() );
    ImGui::PopID();
    return clicked;
}

} // anonymous namespace

void drawToolbarStart( const Session &session ) {
    drawAppBadge();
    std::string const folder = folderName( session.activeDocument().path );
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
    float const padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    float const buttonWidth = iconSize + padding * 2.0f;
    float const buttonsWidth = buttonCount * buttonWidth + ( buttonCount - 1 ) * buttonSpacing;
    std::string const runFile = documentName( session.activeDocument().path );
    float const toolbarWidth = dropdownWidth( runFile ) + buttonSpacing * 4.0f + buttonsWidth;
    ImGui::SetCursorPosX( ImGui::GetWindowWidth() - toolbarWidth - toolbarRightMargin );

    if ( beginDropdown( "##openFiles", runFile, currentScheme().background ) ) {
        for ( const Document &document : session.documents() ) {
            int const id = document.id;
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
