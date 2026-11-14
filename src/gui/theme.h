#pragma once

#include <TextEditor.h>
#include <imgui.h>

#include <span>
#include <string_view>

/// @brief Every colour of one theme.
struct ColorScheme {
    const char *name;
    bool dark;
    ImU32 background, ground, accent;
    ImU32 hovered, selected, border, textSelection, rowStripe;
    ImU32 text, mutedText, secondaryText, codeText;
    ImU32 error, warning, success, changedValue, breakpoint;
    ImU32 lineNumber, currentLineNumber, currentLine;
    ImU32 keyword, declaration, number, string, identifier, knownIdentifier, comment;
    /// Header name of a C #include: <stdio.h>, "mine.h".
    ImU32 headerName;
};

/// @brief Which of the two chosen schemes is used: the desktop's light or dark (System), or
///        always one of them.
enum class ThemeMode { System, Dark, Light };

/// @brief Names of the ThemeMode values, in their order, for the settings file and the UI.
inline constexpr const char *themeModeNames[] = { "System", "Dark", "Light" };

/// @return Colours of the theme in use.
const ColorScheme &currentScheme();

/// @return Every scheme, dark and light.
std::span<const ColorScheme> colorSchemes();

/// @param[in] name Scheme name.
/// @return The scheme with this name, or nullptr.
const ColorScheme *findColorScheme( std::string_view name );

/// @return Whether the dark or the light scheme is used.
ThemeMode themeMode();

/// @brief Switches between the dark and the light scheme and updates the ImGui style.
/// @param[in] mode New mode. System follows the desktop, as last read by followDesktopTheme().
void setThemeMode( ThemeMode mode );

/// @brief Reads the desktop's light or dark setting, which System follows, and updates the
///        ImGui style. Without an answer the last one is kept; dark before any.
void followDesktopTheme();

/// @return Scheme used when the theme is dark.
const ColorScheme &darkColorScheme();

/// @return Scheme used when the theme is light.
const ColorScheme &lightColorScheme();

/// @brief Makes a scheme the dark or the light choice, by its dark flag, and updates the ImGui
///        style.
/// @param[in] scheme An element of colorSchemes().
void setColorScheme( const ColorScheme &scheme );

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

/// @return The accent: selected tabs, switched-on buttons and panel outlines. It is the
///         scheme's own unless the user picked one.
ImU32 accentColor();

/// @return True if the user picked an accent that replaces the scheme's.
bool hasCustomAccent();

/// @brief Goes back to the scheme's own accent and updates the ImGui style.
void resetAccentColor();

/// @return The accent faintly mixed into the panel background: fill of selected and hovered tabs.
ImU32 accentFillColor();

/// @brief Sets an accent that replaces the scheme's in every scheme and updates the ImGui style.
/// @param[in] color New accent; its alpha is ignored.
void setAccentColor( ImU32 color );

/// @return The accent at low alpha: background of the line the debugger is paused on.
ImU32 pausedLineColor();

/// @return Syntax colours of the code editor, on the current panel background.
TextEditor::Palette editorPalette();
