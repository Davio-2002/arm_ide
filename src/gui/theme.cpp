#include <imgui_internal.h>

#include <optional>
#include <string>

#include <core/process.h>
#include <gui/theme.h>

// Defined in color_schemes.cpp; pointers into it are constant-initialized, so the defaults
// below don't depend on static-init order.
extern const ColorScheme colorSchemeTable[];

namespace {

ThemeMode mode = ThemeMode::System;
ThemeMode desktopMode = ThemeMode::Dark;

const ColorScheme *darkScheme = &colorSchemeTable[0];
const ColorScheme *lightScheme = &colorSchemeTable[1];
const ColorScheme *scheme = &colorSchemeTable[0];
std::optional<ImU32> customAccent;

ImVec4 toVec4( ImU32 color ) {
    return ImGui::ColorConvertU32ToFloat4( color );
}

void applySizes( ImGuiStyle &style ) {
    style.WindowPadding = ImVec2( 12.0f, 10.0f );
    style.FramePadding = ImVec2( 8.0f, 5.0f );
    style.ItemSpacing = ImVec2( 8.0f, 6.0f );
    style.WindowRounding = panelRounding;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.TabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBarBorderSize = 0.0f;
    style.TabBarOverlineSize = 0.0f;
    style.DockingSeparatorSize = panelGap;
    style.WindowMenuButtonPosition = ImGuiDir_None;
}

void applyColors( ImGuiStyle &style ) {
    const ColorScheme &colorScheme = *scheme;
    ImU32 scrollbarColor = colorScheme.border;
    ImU32 accent = accentColor();
    ImU32 pressedColor = withAlpha( accent, 80 );

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = toVec4( colorScheme.text );
    colors[ImGuiCol_TextDisabled] = toVec4( colorScheme.mutedText );
    colors[ImGuiCol_WindowBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_ChildBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_PopupBg] = toVec4( colorScheme.selected );
    colors[ImGuiCol_Border] = toVec4( colorScheme.border );
    colors[ImGuiCol_FrameBg] = toVec4( colorScheme.ground );
    colors[ImGuiCol_FrameBgHovered] = toVec4( colorScheme.hovered );
    colors[ImGuiCol_FrameBgActive] = toVec4( colorScheme.selected );
    colors[ImGuiCol_TitleBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_TitleBgActive] = toVec4( colorScheme.background );
    colors[ImGuiCol_TitleBgCollapsed] = toVec4( colorScheme.background );
    colors[ImGuiCol_MenuBarBg] = toVec4( colorScheme.ground );
    colors[ImGuiCol_ScrollbarBg] = toVec4( transparentColor );
    colors[ImGuiCol_ScrollbarGrab] = toVec4( scrollbarColor );
    colors[ImGuiCol_ScrollbarGrabHovered] = toVec4( colorScheme.mutedText );
    colors[ImGuiCol_ScrollbarGrabActive] = toVec4( colorScheme.mutedText );
    colors[ImGuiCol_CheckMark] = toVec4( accent );
    colors[ImGuiCol_SliderGrab] = toVec4( accent );
    colors[ImGuiCol_SliderGrabActive] = toVec4( accent );
    colors[ImGuiCol_Button] = toVec4( colorScheme.selected );
    colors[ImGuiCol_ButtonHovered] = toVec4( colorScheme.border );
    colors[ImGuiCol_ButtonActive] = toVec4( pressedColor );
    colors[ImGuiCol_Header] = toVec4( colorScheme.textSelection );
    colors[ImGuiCol_HeaderHovered] = toVec4( colorScheme.hovered );
    colors[ImGuiCol_HeaderActive] = toVec4( pressedColor );
    colors[ImGuiCol_Separator] = toVec4( colorScheme.border );
    colors[ImGuiCol_SeparatorHovered] = toVec4( accent );
    colors[ImGuiCol_SeparatorActive] = toVec4( accent );
    colors[ImGuiCol_ResizeGrip] = toVec4( transparentColor );
    colors[ImGuiCol_ResizeGripHovered] = toVec4( pressedColor );
    colors[ImGuiCol_ResizeGripActive] = toVec4( accent );
    colors[ImGuiCol_Tab] = toVec4( transparentColor );
    colors[ImGuiCol_TabHovered] = toVec4( colorScheme.hovered );
    colors[ImGuiCol_TabSelected] = toVec4( colorScheme.selected );
    colors[ImGuiCol_TabSelectedOverline] = toVec4( transparentColor );
    colors[ImGuiCol_TabDimmed] = toVec4( transparentColor );
    colors[ImGuiCol_TabDimmedSelected] = toVec4( colorScheme.selected );
    colors[ImGuiCol_TabDimmedSelectedOverline] = toVec4( transparentColor );
    colors[ImGuiCol_DockingPreview] = toVec4( pressedColor );
    colors[ImGuiCol_DockingEmptyBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_TableRowBg] = toVec4( transparentColor );
    colors[ImGuiCol_TableRowBgAlt] = toVec4( colorScheme.rowStripe );
    colors[ImGuiCol_TextSelectedBg] = toVec4( colorScheme.textSelection );
    colors[ImGuiCol_NavCursor] = toVec4( accent );
}

/// @brief Reads `org.freedesktop.appearance color-scheme` from the XDG Desktop Portal.
/// @return Dark or Light, or nullopt without a portal, the setting or `gdbus`.
/// @todo Port to other platforms.
std::optional<ThemeMode> desktopThemeMode() {
    if ( !isInPath( "gdbus" ) ) {
        return std::nullopt;
    }
    CommandResult result = runCommand(
        { "gdbus", "call", "--session", "--dest", "org.freedesktop.portal.Desktop", "--object-path",
          "/org/freedesktop/portal/desktop", "--method", "org.freedesktop.portal.Settings.ReadOne",
          "org.freedesktop.appearance", "color-scheme" },
        "" );
    // Reply "(<uint32 N>,)": 1 dark, 2 light, 0 no preference (light on GNOME).
    const std::string valuePrefix = "uint32 ";
    size_t at = result.output.find( valuePrefix );
    if ( result.exitCode != 0 || at == std::string::npos ) {
        return std::nullopt;
    }
    bool dark = result.output.compare( at + valuePrefix.size(), 1, "1" ) == 0;
    return dark ? ThemeMode::Dark : ThemeMode::Light;
}

/// @brief Picks the scheme by mode (by the desktop for System) and updates the style.
void selectScheme() {
    ThemeMode effectiveMode = mode == ThemeMode::System ? desktopMode : mode;
    scheme = effectiveMode == ThemeMode::Light ? lightScheme : darkScheme;
    applyColors( ImGui::GetStyle() );
}

} // anonymous namespace

const ColorScheme &currentScheme() {
    return *scheme;
}

ThemeMode themeMode() {
    return mode;
}

const ColorScheme &darkColorScheme() {
    return *darkScheme;
}

const ColorScheme &lightColorScheme() {
    return *lightScheme;
}

ImU32 withAlpha( ImU32 color, int alpha ) {
    return ( color & ~IM_COL32_A_MASK ) | ( static_cast<ImU32>( alpha ) << IM_COL32_A_SHIFT );
}

ImU32 accentColor() {
    return customAccent.value_or( scheme->accent );
}

bool hasCustomAccent() {
    return customAccent.has_value();
}

ImU32 pausedLineColor() {
    return withAlpha( accentColor(), 40 );
}

ImU32 accentFillColor() {
    return ImAlphaBlendColors( scheme->background, withAlpha( accentColor(), 46 ) );
}

void applyTheme() {
    ImGuiStyle &style = ImGui::GetStyle();
    applySizes( style );
    applyColors( style );
}

void setThemeMode( ThemeMode newMode ) {
    mode = newMode;
    selectScheme();
}

void followDesktopTheme() {
    if ( std::optional<ThemeMode> desktop = desktopThemeMode() ) {
        desktopMode = *desktop;
    }
    selectScheme();
}

void setColorScheme( const ColorScheme &newScheme ) {
    ( newScheme.dark ? darkScheme : lightScheme ) = &newScheme;
    selectScheme();
}

void setAccentColor( ImU32 color ) {
    customAccent = withAlpha( color, 255 );
    applyColors( ImGui::GetStyle() );
}

void resetAccentColor() {
    customAccent.reset();
    applyColors( ImGui::GetStyle() );
}

TextEditor::Palette editorPalette() {
    TextEditor::Palette palette =
        scheme->dark ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();
    auto set = [&palette]( TextEditor::Color color, ImU32 value ) {
        palette[static_cast<size_t>( color )] = value;
    };
    const ColorScheme &colorScheme = *scheme;

    set( TextEditor::Color::text, colorScheme.codeText );
    set( TextEditor::Color::keyword, colorScheme.keyword );
    set( TextEditor::Color::declaration, colorScheme.declaration );
    set( TextEditor::Color::number, colorScheme.number );
    set( TextEditor::Color::string, colorScheme.string );
    set( TextEditor::Color::punctuation, colorScheme.codeText );
    set( TextEditor::Color::preprocessor, colorScheme.declaration );
    set( TextEditor::Color::identifier, colorScheme.identifier );
    set( TextEditor::Color::knownIdentifier, colorScheme.knownIdentifier );
    set( TextEditor::Color::comment, colorScheme.comment );
    set( TextEditor::Color::customColor1, colorScheme.headerName );
    set( TextEditor::Color::background, colorScheme.background );
    set( TextEditor::Color::cursor, colorScheme.text );
    set( TextEditor::Color::selection, colorScheme.textSelection );
    set( TextEditor::Color::whitespace, colorScheme.border );
    set( TextEditor::Color::matchingBracketBackground, colorScheme.border );
    set( TextEditor::Color::matchingBracketActive, colorScheme.mutedText );
    set( TextEditor::Color::matchingBracketLevel1, colorScheme.declaration );
    set( TextEditor::Color::matchingBracketLevel2, colorScheme.knownIdentifier );
    set( TextEditor::Color::matchingBracketLevel3, colorScheme.identifier );
    set( TextEditor::Color::matchingBracketError, colorScheme.error );
    set( TextEditor::Color::lineNumber, colorScheme.lineNumber );
    set( TextEditor::Color::currentLineNumber, colorScheme.currentLineNumber );
    set( TextEditor::Color::currentLineHighlight, colorScheme.currentLine );
    set( TextEditor::Color::currentLineHighlightBorder, colorScheme.currentLine );
    return palette;
}
