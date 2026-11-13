#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <string>
#include <vector>

#include <app/session.h>
#include <gui/panels/editor.h>
#include <gui/statusbar.h>
#include <gui/theme.h>

static const int breadcrumbDepth = 3;
static const float itemGap = 16.0f;
static const float sidePadding = 14.0f;

/// @brief Shows the last parts of a path, separated by "›".
/// @param[in] path File path; may be empty.
/// @return The last breadcrumbDepth parts, or "untitled" for an empty path.
static std::string breadcrumb( const std::string &path ) {
    if ( path.empty() ) {
        return "untitled";
    }
    std::vector<std::string> parts;
    for ( const std::filesystem::path &part : std::filesystem::path( path ) ) {
        parts.push_back( part.string() );
    }
    size_t first = parts.size() > breadcrumbDepth ? parts.size() - breadcrumbDepth : 0;
    std::string text;
    for ( size_t i = first; i < parts.size(); i++ ) {
        text += ( i == first ? "" : "  ›  " ) + parts[i];
    }
    return text;
}

/// @param[in] session Current session.
/// @return Warning colour while paused, success colour while running, transparent otherwise.
static ImU32 programStateColor( const Session &session ) {
    if ( session.isPaused() ) {
        return warningColor;
    }
    if ( session.isProgramActive() ) {
        return successColor;
    }
    return transparentColor;
}

/// @brief Draws a small dot one text line high.
/// @param[in] color Dot colour.
static void drawStateDot( ImU32 color ) {
    float size = ImGui::GetTextLineHeight();
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImVec2 center( topLeft.x + size * 0.5f, topLeft.y + size * 0.5f );
    ImGui::GetWindowDrawList()->AddCircleFilled( center, size * 0.2f, color );
    ImGui::Dummy( ImVec2( size, size ) );
}

void drawStatusBar( const Session &session ) {
    float height = ImGui::GetFrameHeight();
    float verticalPadding = ( height - ImGui::GetTextLineHeight() ) * 0.5f;
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( sidePadding, verticalPadding ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4.0f, 0.0f ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, groundColor() );
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    if ( ImGui::BeginViewportSideBar( "##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down,
                                      height, flags ) ) {
        ImGui::PushStyleColor( ImGuiCol_Text, secondaryTextColor );
        ImGui::TextUnformatted( breadcrumb( session.filePath ).c_str() );

        std::string cursor =
            std::to_string( editorCursorLine() ) + ":" + std::to_string( editorCursorColumn() );
        std::vector<std::string> facts = { cursor, "LF", "UTF-8", "AArch64" };

        float rightWidth = ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.x +
                           ImGui::CalcTextSize( session.status.c_str() ).x;
        for ( const std::string &fact : facts ) {
            rightWidth += itemGap + ImGui::CalcTextSize( fact.c_str() ).x;
        }
        ImGui::SameLine( ImGui::GetWindowWidth() - rightWidth - sidePadding );
        drawStateDot( programStateColor( session ) );
        ImGui::SameLine();
        ImGui::TextUnformatted( session.status.c_str() );
        for ( const std::string &fact : facts ) {
            ImGui::SameLine( 0.0f, itemGap );
            ImGui::TextUnformatted( fact.c_str() );
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar( 2 );
}
