#pragma once

#include <TextEditor.h>
#include <imgui.h>

/// @brief Every colour of one theme.
struct ColorScheme {
    const char *name;
    bool dark;
    ImU32 background, accent;
    ImU32 hovered, selected, border, textSelection, rowStripe;
    ImU32 text, mutedText, secondaryText, codeText;
    ImU32 error, warning, success, changedValue, breakpoint;
    ImU32 lineNumber, currentLineNumber, currentLine;
    ImU32 keyword, declaration, number, string, identifier, knownIdentifier, comment;
};

/// @return Colours of the theme in use.
const ColorScheme &currentScheme();

/// @param[in] color Colour.
/// @param[in] alpha New alpha, 0 to 255.
/// @return The colour with its alpha replaced.
ImU32 withAlpha( ImU32 color, int alpha );

inline constexpr ImU32 transparentColor = IM_COL32( 0, 0, 0, 0 );

inline constexpr float panelRounding = 10.0f;
inline constexpr float panelGap = 6.0f;
inline constexpr float toolbarFramePaddingY = 9.0f;
inline constexpr float editorTabsGap = 6.0f;

/// @brief Applies the theme sizes and the current scheme's colours to the ImGui style.
void applyTheme();

/// @return The accent: selected tabs, switched-on buttons and panel outlines.
ImU32 accentColor();

/// @return The accent faintly mixed into the panel background: fill of selected and hovered tabs.
ImU32 accentFillColor();

/// @brief Sets the accent colour and updates the ImGui style.
/// @param[in] color New accent; its alpha is ignored.
void setAccentColor( ImU32 color );

/// @return The accent at low alpha: background of the line the debugger is paused on.
ImU32 pausedLineColor();

/// @brief Colour of the frame around the panels: toolbar, status bar, tool strip and gaps.
///
/// It is the panel background at 65% brightness, so the two levels keep their contrast in every
/// scheme.
/// @return Ground colour.
ImU32 groundColor();

/// @return Syntax colours of the code editor, on the current panel background.
TextEditor::Palette editorPalette();
