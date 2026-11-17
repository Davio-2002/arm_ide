#include <imgui.h>
#include <imgui_internal.h>

#include <string>
#include <vector>

#include <gui/appearance.h>
#include <gui/fonts.h>
#include <gui/theme.h>

namespace {

const char *const popupTitle = "Appearance";

struct Appearance {
    bool needsOpening = false;
    std::vector<std::string> uiFonts;
    std::vector<std::string> codeFonts;
};

Appearance appearance;

/// @brief Combo of font families.
/// @param[in]  current Family in use; empty for the built-in font.
/// @param[out] picked  Family clicked.
/// @return True if one was clicked.
bool drawFontCombo( const char *label, const std::vector<std::string> &families,
                    const std::string &current, std::string &picked ) {
    const char *preview = current.empty() ? "Built-in" : current.c_str();
    if ( !ImGui::BeginCombo( label, preview, ImGuiComboFlags_HeightLarge ) ) {
        return false;
    }
    bool clicked = false;
    for ( const std::string &family : families ) {
        bool const isCurrent = family == current;
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

/// @return True if the mode changed.
bool drawThemeMode() {
    bool changed = false;
    for ( size_t i = 0; i < std::size( themeModeNames ); i++ ) {
        if ( i > 0 ) {
            ImGui::SameLine();
        }
        auto mode = static_cast<ThemeMode>( i );
        if ( ImGui::RadioButton( themeModeNames[i], themeMode() == mode ) ) {
            setThemeMode( mode );
            changed = true;
        }
    }
    return changed;
}

/// @brief Combo of the dark or light schemes.
/// @return True if one was clicked.
bool drawSchemeCombo( const char *label, bool dark ) {
    const ColorScheme &current = dark ? darkColorScheme() : lightColorScheme();
    if ( !ImGui::BeginCombo( label, current.name ) ) {
        return false;
    }
    bool clicked = false;
    for ( const ColorScheme &scheme : colorSchemes() ) {
        if ( scheme.dark != dark ) {
            continue;
        }
        bool const isCurrent = &scheme == &current;
        if ( ImGui::Selectable( scheme.name, isCurrent ) ) {
            setColorScheme( scheme );
            clicked = true;
        }
        if ( isCurrent ) {
            ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::EndCombo();
    return clicked;
}

/// @brief Accent colour button, plus Reset for a picked accent.
/// @return True if changed.
bool drawAccent() {
    ImVec4 color = ImGui::ColorConvertU32ToFloat4( accentColor() );
    bool changed = false;
    if ( ImGui::ColorEdit3( "Accent", &color.x, ImGuiColorEditFlags_NoInputs ) ) {
        setAccentColor( ImGui::ColorConvertFloat4ToU32( color ) );
        changed = true;
    }
    if ( hasCustomAccent() ) {
        ImGui::SameLine();
        if ( ImGui::Button( "Reset" ) ) {
            resetAccentColor();
            changed = true;
        }
    }
    return changed;
}

void drawColors() {
    ImGui::SeparatorText( "Colors" );
    bool changed = drawThemeMode();
    changed |= drawSchemeCombo( "Dark scheme", true );
    changed |= drawSchemeCombo( "Light scheme", false );
    changed |= drawAccent();
    if ( changed ) {
        ImGui::MarkIniSettingsDirty();
    }
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
    drawColors();
    ImGui::SeparatorText( "Fonts" );
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
