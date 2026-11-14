#include <imgui_internal.h>

#include <optional>
#include <string>

#include <core/process.h>
#include <gui/theme.h>

namespace {

const ColorScheme schemes[] = {
    {
        .name = "Islands Dark",
        .dark = true,
        .background = IM_COL32( 24, 25, 27, 255 ),
        .ground = IM_COL32( 15, 16, 17, 255 ),
        .accent = IM_COL32( 87, 150, 92, 255 ),
        .hovered = IM_COL32( 38, 40, 43, 255 ),
        .selected = IM_COL32( 43, 45, 48, 255 ),
        .border = IM_COL32( 57, 59, 64, 255 ),
        .textSelection = IM_COL32( 33, 66, 131, 255 ),
        .rowStripe = IM_COL32( 255, 255, 255, 6 ),
        .text = IM_COL32( 223, 225, 229, 255 ),
        .mutedText = IM_COL32( 134, 138, 145, 255 ),
        .secondaryText = IM_COL32( 168, 171, 178, 255 ),
        .codeText = IM_COL32( 188, 190, 196, 255 ),
        .error = IM_COL32( 247, 84, 100, 255 ),
        .warning = IM_COL32( 242, 197, 92, 255 ),
        .success = IM_COL32( 106, 171, 115, 255 ),
        .changedValue = IM_COL32( 242, 197, 92, 255 ),
        .breakpoint = IM_COL32( 219, 92, 92, 255 ),
        .lineNumber = IM_COL32( 75, 80, 89, 255 ),
        .currentLineNumber = IM_COL32( 161, 163, 171, 255 ),
        .currentLine = IM_COL32( 38, 40, 46, 255 ),
        .keyword = IM_COL32( 207, 142, 109, 255 ),
        .declaration = IM_COL32( 179, 174, 96, 255 ),
        .number = IM_COL32( 42, 172, 184, 255 ),
        .string = IM_COL32( 106, 171, 115, 255 ),
        .identifier = IM_COL32( 86, 168, 245, 255 ),
        .knownIdentifier = IM_COL32( 199, 125, 187, 255 ),
        .comment = IM_COL32( 122, 126, 133, 255 ),
    },
    {
        .name = "Islands Light",
        .dark = false,
        .background = IM_COL32( 255, 255, 255, 255 ),
        .ground = IM_COL32( 235, 236, 240, 255 ),
        .accent = IM_COL32( 32, 138, 60, 255 ),
        .hovered = IM_COL32( 240, 241, 243, 255 ),
        .selected = IM_COL32( 235, 236, 240, 255 ),
        .border = IM_COL32( 223, 225, 229, 255 ),
        .textSelection = IM_COL32( 166, 210, 255, 255 ),
        .rowStripe = IM_COL32( 0, 0, 0, 8 ),
        .text = IM_COL32( 8, 8, 8, 255 ),
        .mutedText = IM_COL32( 129, 133, 148, 255 ),
        .secondaryText = IM_COL32( 108, 112, 126, 255 ),
        .codeText = IM_COL32( 8, 8, 8, 255 ),
        .error = IM_COL32( 219, 59, 75, 255 ),
        .warning = IM_COL32( 194, 125, 4, 255 ),
        .success = IM_COL32( 32, 138, 60, 255 ),
        .changedValue = IM_COL32( 194, 125, 4, 255 ),
        .breakpoint = IM_COL32( 219, 59, 75, 255 ),
        .lineNumber = IM_COL32( 174, 179, 194, 255 ),
        .currentLineNumber = IM_COL32( 118, 122, 138, 255 ),
        .currentLine = IM_COL32( 245, 248, 254, 255 ),
        .keyword = IM_COL32( 0, 51, 179, 255 ),
        .declaration = IM_COL32( 158, 136, 13, 255 ),
        .number = IM_COL32( 23, 80, 235, 255 ),
        .string = IM_COL32( 6, 125, 23, 255 ),
        .identifier = IM_COL32( 0, 98, 122, 255 ),
        .knownIdentifier = IM_COL32( 135, 16, 148, 255 ),
        .comment = IM_COL32( 140, 140, 140, 255 ),
    },
    // Catppuccin, https://github.com/catppuccin/catppuccin, MIT licence.
    {
        .name = "Catppuccin Mocha",
        .dark = true,
        .background = IM_COL32( 30, 30, 46, 255 ),
        .ground = IM_COL32( 17, 17, 27, 255 ),
        .accent = IM_COL32( 203, 166, 247, 255 ),
        .hovered = IM_COL32( 42, 43, 60, 255 ),
        .selected = IM_COL32( 49, 50, 68, 255 ),
        .border = IM_COL32( 69, 71, 90, 255 ),
        .textSelection = IM_COL32( 147, 153, 178, 70 ),
        .rowStripe = IM_COL32( 255, 255, 255, 6 ),
        .text = IM_COL32( 205, 214, 244, 255 ),
        .mutedText = IM_COL32( 127, 132, 156, 255 ),
        .secondaryText = IM_COL32( 166, 173, 200, 255 ),
        .codeText = IM_COL32( 205, 214, 244, 255 ),
        .error = IM_COL32( 243, 139, 168, 255 ),
        .warning = IM_COL32( 249, 226, 175, 255 ),
        .success = IM_COL32( 166, 227, 161, 255 ),
        .changedValue = IM_COL32( 249, 226, 175, 255 ),
        .breakpoint = IM_COL32( 243, 139, 168, 255 ),
        .lineNumber = IM_COL32( 108, 112, 134, 255 ),
        .currentLineNumber = IM_COL32( 180, 190, 254, 255 ),
        .currentLine = IM_COL32( 39, 40, 57, 255 ),
        .keyword = IM_COL32( 203, 166, 247, 255 ),
        .declaration = IM_COL32( 249, 226, 175, 255 ),
        .number = IM_COL32( 250, 179, 135, 255 ),
        .string = IM_COL32( 166, 227, 161, 255 ),
        .identifier = IM_COL32( 137, 180, 250, 255 ),
        .knownIdentifier = IM_COL32( 148, 226, 213, 255 ),
        .comment = IM_COL32( 147, 153, 178, 255 ),
    },
    {
        .name = "Catppuccin Latte",
        .dark = false,
        .background = IM_COL32( 239, 241, 245, 255 ),
        .ground = IM_COL32( 220, 224, 232, 255 ),
        .accent = IM_COL32( 136, 57, 239, 255 ),
        .hovered = IM_COL32( 230, 233, 239, 255 ),
        .selected = IM_COL32( 220, 224, 232, 255 ),
        .border = IM_COL32( 204, 208, 218, 255 ),
        .textSelection = IM_COL32( 124, 127, 147, 70 ),
        .rowStripe = IM_COL32( 0, 0, 0, 8 ),
        .text = IM_COL32( 76, 79, 105, 255 ),
        .mutedText = IM_COL32( 140, 143, 161, 255 ),
        .secondaryText = IM_COL32( 108, 111, 133, 255 ),
        .codeText = IM_COL32( 76, 79, 105, 255 ),
        .error = IM_COL32( 210, 15, 57, 255 ),
        .warning = IM_COL32( 223, 142, 29, 255 ),
        .success = IM_COL32( 64, 160, 43, 255 ),
        .changedValue = IM_COL32( 223, 142, 29, 255 ),
        .breakpoint = IM_COL32( 210, 15, 57, 255 ),
        .lineNumber = IM_COL32( 156, 160, 176, 255 ),
        .currentLineNumber = IM_COL32( 114, 135, 253, 255 ),
        .currentLine = IM_COL32( 230, 233, 239, 255 ),
        .keyword = IM_COL32( 136, 57, 239, 255 ),
        .declaration = IM_COL32( 223, 142, 29, 255 ),
        .number = IM_COL32( 254, 100, 11, 255 ),
        .string = IM_COL32( 64, 160, 43, 255 ),
        .identifier = IM_COL32( 30, 102, 245, 255 ),
        .knownIdentifier = IM_COL32( 23, 146, 153, 255 ),
        .comment = IM_COL32( 124, 127, 147, 255 ),
    },
    // Gruvbox, https://github.com/morhetz/gruvbox, MIT licence.
    {
        .name = "Gruvbox Dark",
        .dark = true,
        .background = IM_COL32( 40, 40, 40, 255 ),
        .ground = IM_COL32( 29, 32, 33, 255 ),
        .accent = IM_COL32( 254, 128, 25, 255 ),
        .hovered = IM_COL32( 50, 48, 47, 255 ),
        .selected = IM_COL32( 60, 56, 54, 255 ),
        .border = IM_COL32( 80, 73, 69, 255 ),
        .textSelection = IM_COL32( 80, 73, 69, 255 ),
        .rowStripe = IM_COL32( 255, 255, 255, 6 ),
        .text = IM_COL32( 235, 219, 178, 255 ),
        .mutedText = IM_COL32( 146, 131, 116, 255 ),
        .secondaryText = IM_COL32( 168, 153, 132, 255 ),
        .codeText = IM_COL32( 235, 219, 178, 255 ),
        .error = IM_COL32( 251, 73, 52, 255 ),
        .warning = IM_COL32( 250, 189, 47, 255 ),
        .success = IM_COL32( 184, 187, 38, 255 ),
        .changedValue = IM_COL32( 250, 189, 47, 255 ),
        .breakpoint = IM_COL32( 251, 73, 52, 255 ),
        .lineNumber = IM_COL32( 124, 111, 100, 255 ),
        .currentLineNumber = IM_COL32( 250, 189, 47, 255 ),
        .currentLine = IM_COL32( 50, 48, 47, 255 ),
        .keyword = IM_COL32( 251, 73, 52, 255 ),
        .declaration = IM_COL32( 142, 192, 124, 255 ),
        .number = IM_COL32( 211, 134, 155, 255 ),
        .string = IM_COL32( 184, 187, 38, 255 ),
        .identifier = IM_COL32( 131, 165, 152, 255 ),
        .knownIdentifier = IM_COL32( 250, 189, 47, 255 ),
        .comment = IM_COL32( 146, 131, 116, 255 ),
    },
    {
        .name = "Gruvbox Light",
        .dark = false,
        .background = IM_COL32( 251, 241, 199, 255 ),
        .ground = IM_COL32( 235, 219, 178, 255 ),
        .accent = IM_COL32( 175, 58, 3, 255 ),
        .hovered = IM_COL32( 242, 229, 188, 255 ),
        .selected = IM_COL32( 235, 219, 178, 255 ),
        .border = IM_COL32( 213, 196, 161, 255 ),
        .textSelection = IM_COL32( 213, 196, 161, 255 ),
        .rowStripe = IM_COL32( 0, 0, 0, 8 ),
        .text = IM_COL32( 60, 56, 54, 255 ),
        .mutedText = IM_COL32( 146, 131, 116, 255 ),
        .secondaryText = IM_COL32( 124, 111, 100, 255 ),
        .codeText = IM_COL32( 60, 56, 54, 255 ),
        .error = IM_COL32( 157, 0, 6, 255 ),
        .warning = IM_COL32( 181, 118, 20, 255 ),
        .success = IM_COL32( 121, 116, 14, 255 ),
        .changedValue = IM_COL32( 181, 118, 20, 255 ),
        .breakpoint = IM_COL32( 204, 36, 29, 255 ),
        .lineNumber = IM_COL32( 168, 153, 132, 255 ),
        .currentLineNumber = IM_COL32( 181, 118, 20, 255 ),
        .currentLine = IM_COL32( 242, 229, 188, 255 ),
        .keyword = IM_COL32( 157, 0, 6, 255 ),
        .declaration = IM_COL32( 66, 123, 88, 255 ),
        .number = IM_COL32( 143, 63, 113, 255 ),
        .string = IM_COL32( 121, 116, 14, 255 ),
        .identifier = IM_COL32( 7, 102, 120, 255 ),
        .knownIdentifier = IM_COL32( 181, 118, 20, 255 ),
        .comment = IM_COL32( 146, 131, 116, 255 ),
    },
};

ThemeMode mode = ThemeMode::System;
ThemeMode desktopMode = ThemeMode::Dark;

const ColorScheme *darkScheme = &schemes[0];
const ColorScheme *lightScheme = &schemes[1];
const ColorScheme *scheme = darkScheme;
std::optional<ImU32> customAccent;

} // anonymous namespace

const ColorScheme &currentScheme() {
    return *scheme;
}

std::span<const ColorScheme> colorSchemes() {
    return schemes;
}

const ColorScheme *findColorScheme( std::string_view name ) {
    for ( const ColorScheme &candidate : schemes ) {
        if ( name == candidate.name ) {
            return &candidate;
        }
    }
    return nullptr;
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

namespace {

/// @param[in] color Packed colour.
/// @return The colour as floats, as ImGuiStyle stores it.
ImVec4 toVec4( ImU32 color ) {
    return ImGui::ColorConvertU32ToFloat4( color );
}

/// @brief Sets paddings, roundings and borders of the theme.
/// @param[in,out] style Style to change.
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

/// @brief Sets the ImGui colours from the current scheme and accent.
/// @param[in,out] style Style to change.
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

} // anonymous namespace

void applyTheme() {
    ImGuiStyle &style = ImGui::GetStyle();
    applySizes( style );
    applyColors( style );
}

namespace {

/// @brief Reads the desktop's light or dark setting from the XDG Desktop Portal
///        (`org.freedesktop.appearance color-scheme`), which GNOME, KDE and the GTK portal
///        provide.
/// @return Dark or Light, or nullopt if there is no portal, no such setting or no `gdbus`.
/// @todo Think about this, how this will be ported to other platforms ??
std::optional<ThemeMode> desktopThemeMode() {
    if ( !isInPath( "gdbus" ) ) {
        return std::nullopt;
    }
    CommandResult result = runCommand(
        { "gdbus", "call", "--session", "--dest", "org.freedesktop.portal.Desktop", "--object-path",
          "/org/freedesktop/portal/desktop", "--method", "org.freedesktop.portal.Settings.ReadOne",
          "org.freedesktop.appearance", "color-scheme" },
        "" );
    // The reply is "(<uint32 N>,)": 1 is dark, 2 is light, 0 is no preference (light on GNOME).
    const std::string valuePrefix = "uint32 ";
    size_t at = result.output.find( valuePrefix );
    if ( result.exitCode != 0 || at == std::string::npos ) {
        return std::nullopt;
    }
    bool dark = result.output.compare( at + valuePrefix.size(), 1, "1" ) == 0;
    return dark ? ThemeMode::Dark : ThemeMode::Light;
}

/// @brief Picks the dark or the light scheme by the mode, or by the desktop for System, and
///        updates the ImGui style.
void selectScheme() {
    ThemeMode effectiveMode = mode == ThemeMode::System ? desktopMode : mode;
    scheme = effectiveMode == ThemeMode::Light ? lightScheme : darkScheme;
    applyColors( ImGui::GetStyle() );
}

} // anonymous namespace

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
