#include <imgui_internal.h>

#include <gui/theme.h>

namespace {

const ColorScheme schemes[] = {
    {
        .name = "Islands Dark",
        .dark = true,
        .background = IM_COL32( 24, 25, 27, 255 ),
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
};

const ColorScheme *scheme = &schemes[0];
ImU32 accent = scheme->accent;

} // anonymous namespace

const ColorScheme &currentScheme() {
    return *scheme;
}

ImU32 withAlpha( ImU32 color, int alpha ) {
    return ( color & ~IM_COL32_A_MASK ) | ( static_cast<ImU32>( alpha ) << IM_COL32_A_SHIFT );
}

ImU32 accentColor() {
    return accent;
}

ImU32 pausedLineColor() {
    return withAlpha( accent, 40 );
}

ImU32 accentFillColor() {
    return ImAlphaBlendColors( scheme->background, withAlpha( accent, 46 ) );
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
    ImU32 pressedColor = withAlpha( accent, 80 );

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = toVec4( colorScheme.text );
    colors[ImGuiCol_TextDisabled] = toVec4( colorScheme.mutedText );
    colors[ImGuiCol_WindowBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_ChildBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_PopupBg] = toVec4( colorScheme.selected );
    colors[ImGuiCol_Border] = toVec4( colorScheme.border );
    colors[ImGuiCol_FrameBg] = toVec4( groundColor() );
    colors[ImGuiCol_FrameBgHovered] = toVec4( colorScheme.hovered );
    colors[ImGuiCol_FrameBgActive] = toVec4( colorScheme.selected );
    colors[ImGuiCol_TitleBg] = toVec4( colorScheme.background );
    colors[ImGuiCol_TitleBgActive] = toVec4( colorScheme.background );
    colors[ImGuiCol_TitleBgCollapsed] = toVec4( colorScheme.background );
    colors[ImGuiCol_MenuBarBg] = toVec4( groundColor() );
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

void setAccentColor( ImU32 color ) {
    accent = withAlpha( color, 255 );
    applyColors( ImGui::GetStyle() );
}

ImU32 groundColor() {
    return ImAlphaBlendColors( scheme->background, IM_COL32( 0, 0, 0, 89 ) );
}

TextEditor::Palette editorPalette() {
    TextEditor::Palette palette = TextEditor::GetDarkPalette();
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
