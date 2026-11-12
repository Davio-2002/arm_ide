#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

#include <app/session.h>
#include <gui/icons.h>
#include <gui/menu.h>
#include <gui/panels/editor.h>
#include <gui/theme.h>
#include <gui/toolbar.h>
#include <gui/widgets.h>

static const int buttonCount = 4;
static const float buttonSpacing = 2.0f;
static const float dropdownPadding = 10.0f;
static const float chevronSize = 8.0f;
static const float badgeSize = 26.0f;
static const float badgeFontSize = 11.0f;
static const float toolbarRightMargin = 32.0f;

static void drawChevronDown( ImVec2 center, ImU32 color ) {
    float half = chevronSize * 0.5f;
    ImVec2 points[] = { ImVec2( center.x - half, center.y - half * 0.5f ),
                        ImVec2( center.x, center.y + half * 0.5f ),
                        ImVec2( center.x + half, center.y - half * 0.5f ) };
    ImGui::GetWindowDrawList()->AddPolyline( points, 3, color, ImDrawFlags_None, 1.5f );
}

static float dropdownWidth( const std::string &label ) {
    return ImGui::CalcTextSize( label.c_str() ).x + chevronSize + dropdownPadding * 3.0f;
}

// A label with a down arrow that opens the popup named id below it. Returns true while the
// popup is open, between it and ImGui::EndPopup.
static bool beginDropdown( const char *id, const std::string &label, ImU32 background ) {
    ImVec2 size( dropdownWidth( label ), ImGui::GetFrameHeight() );
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( id, size ) ) {
        ImGui::OpenPopup( id );
    }

    ImU32 fill = ImGui::IsItemHovered() ? accentFillColor() : background;
    ImVec2 bottomRight( topLeft.x + size.x, topLeft.y + size.y );
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled( topLeft, bottomRight, fill, ImGui::GetStyle().FrameRounding );
    float textY = topLeft.y + ( size.y - ImGui::GetTextLineHeight() ) * 0.5f;
    drawList->AddText( ImVec2( topLeft.x + dropdownPadding, textY ), textColor, label.c_str() );
    ImVec2 chevronCenter( bottomRight.x - dropdownPadding - chevronSize * 0.5f,
                          topLeft.y + size.y * 0.5f );
    drawChevronDown( chevronCenter, mutedTextColor );

    ImGui::SetNextWindowPos( ImVec2( topLeft.x, bottomRight.y + 4.0f ) );
    return ImGui::BeginPopup( id );
}

static void drawAppBadge() {
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

static std::string fileName( const std::string &path ) {
    return path.empty() ? "untitled" : std::filesystem::path( path ).filename().string();
}

static std::string folderName( const std::string &path ) {
    if ( path.empty() ) {
        return "No folder";
    }
    return std::filesystem::path( path ).parent_path().filename().string();
}

static void drawFileList( const std::vector<std::string> &paths, MenuActions &actions ) {
    for ( const std::string &path : paths ) {
        ImGui::PushID( path.c_str() );
        if ( ImGui::Selectable( fileName( path ).c_str() ) ) {
            actions.fileToOpen = path;
        }
        ImGui::SetItemTooltip( "%s", displayPath( path ).c_str() );
        ImGui::PopID();
    }
}

void drawToolbarStart( const Session &session, MenuActions &actions ) {
    drawAppBadge();
    if ( beginDropdown( "##recentFiles", folderName( session.filePath ), transparentColor ) ) {
        if ( session.recentFiles.empty() ) {
            ImGui::TextDisabled( "No recent files" );
        }
        drawFileList( session.recentFiles, actions );
        ImGui::EndPopup();
    }
    drawVerticalSeparator();
}

void drawToolbarEnd( const Session &session, MenuActions &actions ) {
    float padding = ( ImGui::GetFrameHeight() - iconSize ) * 0.5f;
    float buttonWidth = iconSize + padding * 2.0f;
    float buttonsWidth = buttonCount * buttonWidth + ( buttonCount - 1 ) * buttonSpacing;
    std::string runFile = fileName( session.filePath );
    float toolbarWidth = dropdownWidth( runFile ) + buttonSpacing * 4.0f + buttonsWidth;
    ImGui::SetCursorPosX( ImGui::GetWindowWidth() - toolbarWidth - toolbarRightMargin );

    if ( beginDropdown( "##openFiles", runFile, islandColor() ) ) {
        drawFileList( openDocumentPaths(), actions );
        ImGui::EndPopup();
    }
    ImGui::SameLine( 0.0f, buttonSpacing * 4.0f );

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( padding, padding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( buttonSpacing, 0.0f ) );
    actions.build |= iconButton( "##build", Icon::Build, "Build  F7" );
    actions.run |= iconButton( "##run", Icon::Run, "Build and Run  F5" );
    actions.debugContinue |= iconButton( "##debug", Icon::Debug, "Debug  F8" );
    actions.stop |= iconButton( "##stop", Icon::Stop, "Stop  Shift+F5", session.isProgramActive() );
    ImGui::PopStyleVar( 2 );
}
