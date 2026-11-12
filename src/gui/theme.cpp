#include <imgui_internal.h>

#include <gui/theme.h>

static ImU32 accent = defaultAccentColor;
static ImU32 island = defaultIslandColor;

ImU32 accentColor() {
    return accent;
}

static ImU32 withAlpha( ImU32 color, int alpha ) {
    return ( color & ~IM_COL32_A_MASK ) | ( static_cast<ImU32>( alpha ) << IM_COL32_A_SHIFT );
}

ImU32 accentFillColor() {
    return ImAlphaBlendColors( island, withAlpha( accent, 46 ) );
}

static ImVec4 toVec4( ImU32 color ) {
    return ImGui::ColorConvertU32ToFloat4( color );
}

static void applySizes( ImGuiStyle &style ) {
    style.WindowPadding = ImVec2( 12.0f, 10.0f );
    style.FramePadding = ImVec2( 8.0f, 5.0f );
    style.ItemSpacing = ImVec2( 8.0f, 6.0f );
    style.WindowRounding = islandRounding;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.TabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBarBorderSize = 0.0f;
    style.TabBarOverlineSize = 0.0f;
    style.DockingSeparatorSize = islandGap;
    style.WindowMenuButtonPosition = ImGuiDir_None;
}

static void applyColors( ImGuiStyle &style ) {
    ImU32 scrollbarColor = borderColor;
    ImU32 pressedColor = withAlpha( accent, 80 );

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = toVec4( textColor );
    colors[ImGuiCol_TextDisabled] = toVec4( mutedTextColor );
    colors[ImGuiCol_WindowBg] = toVec4( island );
    colors[ImGuiCol_ChildBg] = toVec4( island );
    colors[ImGuiCol_PopupBg] = toVec4( selectedColor );
    colors[ImGuiCol_Border] = toVec4( borderColor );
    colors[ImGuiCol_FrameBg] = toVec4( groundColor() );
    colors[ImGuiCol_FrameBgHovered] = toVec4( hoveredColor );
    colors[ImGuiCol_FrameBgActive] = toVec4( selectedColor );
    colors[ImGuiCol_TitleBg] = toVec4( island );
    colors[ImGuiCol_TitleBgActive] = toVec4( island );
    colors[ImGuiCol_TitleBgCollapsed] = toVec4( island );
    colors[ImGuiCol_MenuBarBg] = toVec4( groundColor() );
    colors[ImGuiCol_ScrollbarBg] = toVec4( transparentColor );
    colors[ImGuiCol_ScrollbarGrab] = toVec4( scrollbarColor );
    colors[ImGuiCol_ScrollbarGrabHovered] = toVec4( mutedTextColor );
    colors[ImGuiCol_ScrollbarGrabActive] = toVec4( mutedTextColor );
    colors[ImGuiCol_CheckMark] = toVec4( accent );
    colors[ImGuiCol_SliderGrab] = toVec4( accent );
    colors[ImGuiCol_SliderGrabActive] = toVec4( accent );
    colors[ImGuiCol_Button] = toVec4( selectedColor );
    colors[ImGuiCol_ButtonHovered] = toVec4( borderColor );
    colors[ImGuiCol_ButtonActive] = toVec4( pressedColor );
    colors[ImGuiCol_Header] = toVec4( textSelectionColor );
    colors[ImGuiCol_HeaderHovered] = toVec4( hoveredColor );
    colors[ImGuiCol_HeaderActive] = toVec4( pressedColor );
    colors[ImGuiCol_Separator] = toVec4( borderColor );
    colors[ImGuiCol_SeparatorHovered] = toVec4( accent );
    colors[ImGuiCol_SeparatorActive] = toVec4( accent );
    colors[ImGuiCol_ResizeGrip] = toVec4( transparentColor );
    colors[ImGuiCol_ResizeGripHovered] = toVec4( pressedColor );
    colors[ImGuiCol_ResizeGripActive] = toVec4( accent );
    colors[ImGuiCol_Tab] = toVec4( transparentColor );
    colors[ImGuiCol_TabHovered] = toVec4( hoveredColor );
    colors[ImGuiCol_TabSelected] = toVec4( selectedColor );
    colors[ImGuiCol_TabSelectedOverline] = toVec4( transparentColor );
    colors[ImGuiCol_TabDimmed] = toVec4( transparentColor );
    colors[ImGuiCol_TabDimmedSelected] = toVec4( selectedColor );
    colors[ImGuiCol_TabDimmedSelectedOverline] = toVec4( transparentColor );
    colors[ImGuiCol_DockingPreview] = toVec4( pressedColor );
    colors[ImGuiCol_DockingEmptyBg] = toVec4( island );
    colors[ImGuiCol_TableRowBg] = toVec4( transparentColor );
    colors[ImGuiCol_TableRowBgAlt] = toVec4( IM_COL32( 255, 255, 255, 6 ) );
    colors[ImGuiCol_TextSelectedBg] = toVec4( textSelectionColor );
    colors[ImGuiCol_NavCursor] = toVec4( accent );
}

void applyIslandsTheme() {
    ImGuiStyle &style = ImGui::GetStyle();
    applySizes( style );
    applyColors( style );
}

void setAccentColor( ImU32 color ) {
    accent = withAlpha( color, 255 );
    applyColors( ImGui::GetStyle() );
}

ImU32 islandColor() {
    return island;
}

ImU32 groundColor() {
    return ImAlphaBlendColors( island, IM_COL32( 0, 0, 0, 89 ) );
}

void setIslandColor( ImU32 color ) {
    island = withAlpha( color, 255 );
    applyColors( ImGui::GetStyle() );
}

TextEditor::Palette editorPalette() {
    TextEditor::Palette palette = TextEditor::GetDarkPalette();
    auto set = [&palette]( TextEditor::Color color, ImU32 value ) {
        palette[static_cast<size_t>( color )] = value;
    };
    // The JetBrains "Dark" scheme that Islands Dark uses.
    ImU32 keywordOrange = IM_COL32( 207, 142, 109, 255 );
    ImU32 metadataYellow = IM_COL32( 179, 174, 96, 255 );
    ImU32 numberCyan = IM_COL32( 42, 172, 184, 255 );
    ImU32 stringGreen = IM_COL32( 106, 171, 115, 255 );
    ImU32 functionBlue = IM_COL32( 86, 168, 245, 255 );
    ImU32 fieldPurple = IM_COL32( 199, 125, 187, 255 );
    ImU32 commentGrey = IM_COL32( 122, 126, 133, 255 );
    ImU32 faintColor = borderColor;

    set( TextEditor::Color::text, codeTextColor );
    set( TextEditor::Color::keyword, keywordOrange );
    set( TextEditor::Color::declaration, metadataYellow );
    set( TextEditor::Color::number, numberCyan );
    set( TextEditor::Color::string, stringGreen );
    set( TextEditor::Color::punctuation, codeTextColor );
    set( TextEditor::Color::preprocessor, metadataYellow );
    set( TextEditor::Color::identifier, functionBlue );
    set( TextEditor::Color::knownIdentifier, fieldPurple );
    set( TextEditor::Color::comment, commentGrey );
    set( TextEditor::Color::background, island );
    set( TextEditor::Color::cursor, textColor );
    set( TextEditor::Color::selection, textSelectionColor );
    set( TextEditor::Color::whitespace, faintColor );
    set( TextEditor::Color::matchingBracketBackground, faintColor );
    set( TextEditor::Color::matchingBracketActive, mutedTextColor );
    set( TextEditor::Color::matchingBracketLevel1, metadataYellow );
    set( TextEditor::Color::matchingBracketLevel2, fieldPurple );
    set( TextEditor::Color::matchingBracketLevel3, functionBlue );
    set( TextEditor::Color::matchingBracketError, errorColor );
    set( TextEditor::Color::lineNumber, IM_COL32( 75, 80, 89, 255 ) );
    set( TextEditor::Color::currentLineNumber, IM_COL32( 161, 163, 171, 255 ) );
    set( TextEditor::Color::currentLineHighlight, IM_COL32( 38, 40, 46, 255 ) );
    set( TextEditor::Color::currentLineHighlightBorder, IM_COL32( 38, 40, 46, 255 ) );
    return palette;
}
