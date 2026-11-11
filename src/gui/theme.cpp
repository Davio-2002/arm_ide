#include <gui/theme.h>

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
    ImU32 pressedColor = IM_COL32( 53, 116, 240, 80 );

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = toVec4( textColor );
    colors[ImGuiCol_TextDisabled] = toVec4( mutedTextColor );
    colors[ImGuiCol_WindowBg] = toVec4( islandColor );
    colors[ImGuiCol_ChildBg] = toVec4( islandColor );
    colors[ImGuiCol_PopupBg] = toVec4( selectedColor );
    colors[ImGuiCol_Border] = toVec4( borderColor );
    colors[ImGuiCol_FrameBg] = toVec4( groundColor );
    colors[ImGuiCol_FrameBgHovered] = toVec4( hoveredColor );
    colors[ImGuiCol_FrameBgActive] = toVec4( selectedColor );
    colors[ImGuiCol_TitleBg] = toVec4( islandColor );
    colors[ImGuiCol_TitleBgActive] = toVec4( islandColor );
    colors[ImGuiCol_TitleBgCollapsed] = toVec4( islandColor );
    colors[ImGuiCol_MenuBarBg] = toVec4( groundColor );
    colors[ImGuiCol_ScrollbarBg] = toVec4( transparentColor );
    colors[ImGuiCol_ScrollbarGrab] = toVec4( scrollbarColor );
    colors[ImGuiCol_ScrollbarGrabHovered] = toVec4( mutedTextColor );
    colors[ImGuiCol_ScrollbarGrabActive] = toVec4( mutedTextColor );
    colors[ImGuiCol_CheckMark] = toVec4( accentColor );
    colors[ImGuiCol_SliderGrab] = toVec4( accentColor );
    colors[ImGuiCol_SliderGrabActive] = toVec4( accentColor );
    colors[ImGuiCol_Button] = toVec4( selectedColor );
    colors[ImGuiCol_ButtonHovered] = toVec4( borderColor );
    colors[ImGuiCol_ButtonActive] = toVec4( pressedColor );
    colors[ImGuiCol_Header] = toVec4( selectedColor );
    colors[ImGuiCol_HeaderHovered] = toVec4( hoveredColor );
    colors[ImGuiCol_HeaderActive] = toVec4( pressedColor );
    colors[ImGuiCol_Separator] = toVec4( borderColor );
    colors[ImGuiCol_SeparatorHovered] = toVec4( accentColor );
    colors[ImGuiCol_SeparatorActive] = toVec4( accentColor );
    colors[ImGuiCol_ResizeGrip] = toVec4( transparentColor );
    colors[ImGuiCol_ResizeGripHovered] = toVec4( pressedColor );
    colors[ImGuiCol_ResizeGripActive] = toVec4( accentColor );
    colors[ImGuiCol_Tab] = toVec4( transparentColor );
    colors[ImGuiCol_TabHovered] = toVec4( hoveredColor );
    colors[ImGuiCol_TabSelected] = toVec4( selectedColor );
    colors[ImGuiCol_TabSelectedOverline] = toVec4( transparentColor );
    colors[ImGuiCol_TabDimmed] = toVec4( transparentColor );
    colors[ImGuiCol_TabDimmedSelected] = toVec4( selectedColor );
    colors[ImGuiCol_TabDimmedSelectedOverline] = toVec4( transparentColor );
    colors[ImGuiCol_DockingPreview] = toVec4( pressedColor );
    colors[ImGuiCol_DockingEmptyBg] = toVec4( islandColor );
    colors[ImGuiCol_TableRowBg] = toVec4( transparentColor );
    colors[ImGuiCol_TableRowBgAlt] = toVec4( IM_COL32( 255, 255, 255, 6 ) );
    colors[ImGuiCol_TextSelectedBg] = toVec4( pressedColor );
    colors[ImGuiCol_NavCursor] = toVec4( accentColor );
}

void applyIslandsTheme() {
    ImGuiStyle &style = ImGui::GetStyle();
    applySizes( style );
    applyColors( style );
}

TextEditor::Palette editorPalette() {
    TextEditor::Palette palette = TextEditor::GetDarkPalette();
    auto set = [&palette]( TextEditor::Color color, ImU32 value ) {
        palette[static_cast<size_t>( color )] = value;
    };
    ImU32 faintColor = borderColor;

    set( TextEditor::Color::text, codeTextColor );
    set( TextEditor::Color::keyword, IM_COL32( 232, 145, 74, 255 ) );
    set( TextEditor::Color::declaration, IM_COL32( 224, 105, 74, 255 ) );
    set( TextEditor::Color::number, IM_COL32( 211, 134, 155, 255 ) );
    set( TextEditor::Color::string, IM_COL32( 216, 176, 74, 255 ) );
    set( TextEditor::Color::punctuation, IM_COL32( 168, 171, 178, 255 ) );
    set( TextEditor::Color::preprocessor, IM_COL32( 131, 165, 152, 255 ) );
    set( TextEditor::Color::identifier, IM_COL32( 233, 180, 76, 255 ) );
    set( TextEditor::Color::knownIdentifier, IM_COL32( 169, 182, 101, 255 ) );
    set( TextEditor::Color::comment, IM_COL32( 122, 126, 133, 255 ) );
    set( TextEditor::Color::background, islandColor );
    set( TextEditor::Color::cursor, textColor );
    set( TextEditor::Color::selection, IM_COL32( 33, 66, 131, 255 ) );
    set( TextEditor::Color::whitespace, faintColor );
    set( TextEditor::Color::matchingBracketBackground, faintColor );
    set( TextEditor::Color::matchingBracketActive, mutedTextColor );
    set( TextEditor::Color::matchingBracketLevel1, IM_COL32( 233, 180, 76, 255 ) );
    set( TextEditor::Color::matchingBracketLevel2, IM_COL32( 131, 165, 152, 255 ) );
    set( TextEditor::Color::matchingBracketLevel3, IM_COL32( 211, 134, 155, 255 ) );
    set( TextEditor::Color::matchingBracketError, errorColor );
    set( TextEditor::Color::lineNumber, IM_COL32( 94, 97, 104, 255 ) );
    set( TextEditor::Color::currentLineNumber, IM_COL32( 161, 163, 171, 255 ) );
    set( TextEditor::Color::currentLineHighlight, IM_COL32( 255, 255, 255, 8 ) );
    set( TextEditor::Color::currentLineHighlightBorder, IM_COL32( 255, 255, 255, 14 ) );
    return palette;
}
