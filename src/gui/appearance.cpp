#include <imgui.h>

#include <string>
#include <vector>

#include <core/fonts.h>
#include <gui/appearance.h>
#include <gui/window.h>

namespace {

const char *const popupTitle = "Appearance";

/// @brief State of the Appearance popup while it is open.
struct Appearance {
    bool needsOpening = false;
    std::vector<std::string> uiFonts;
    std::vector<std::string> codeFonts;
};

Appearance appearance;

/// @brief Draws a combo box listing font families.
/// @param[in]  label    Combo label.
/// @param[in]  families Families to list.
/// @param[in]  current  Family in use; empty for ImGui's built-in font.
/// @param[out] picked   Set to the family clicked; unchanged otherwise.
/// @return True if a family was clicked.
bool drawFontCombo( const char *label, const std::vector<std::string> &families,
                    const std::string &current, std::string &picked ) {
    const char *preview = current.empty() ? "Built-in" : current.c_str();
    if ( !ImGui::BeginCombo( label, preview, ImGuiComboFlags_HeightLarge ) ) {
        return false;
    }
    bool clicked = false;
    for ( const std::string &family : families ) {
        bool isCurrent = family == current;
        if ( ImGui::Selectable( family.c_str(), isCurrent ) ) {
            picked = family;
            clicked = true;
        }
        if ( isCurrent ) {
            ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::EndCombo();
    return clicked;
}

} // anonymous namespace

void openAppearance() {
    appearance.needsOpening = true;
    appearance.uiFonts = fontFamilies( false );
    appearance.codeFonts = fontFamilies( true );
}

void drawAppearance() {
    if ( appearance.needsOpening ) {
        ImGui::OpenPopup( popupTitle );
        appearance.needsOpening = false;
    }
    ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                             ImVec2( 0.5f, 0.5f ) );
    if ( !ImGui::BeginPopup( popupTitle, ImGuiWindowFlags_AlwaysAutoResize ) ) {
        return;
    }
    std::string picked;
    if ( drawFontCombo( "Interface font", appearance.uiFonts, uiFontFamily(), picked ) ) {
        setUiFontFamily( picked );
    }
    if ( drawFontCombo( "Code font", appearance.codeFonts, codeFontFamily(), picked ) ) {
        setCodeFontFamily( picked );
    }
    ImGui::Separator();
    if ( ImGui::Button( "Close" ) || ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
