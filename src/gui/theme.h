#pragma once

#include <TextEditor.h>
#include <imgui.h>

#include <array>
#include <span>
#include <string_view>

struct ColorScheme {
    const char *name;
    bool dark;
    ImU32 background, ground, accent;
    ImU32 hovered, selected, border, textSelection, rowStripe;
    ImU32 text, mutedText, secondaryText, codeText;
    ImU32 error, warning, success, changedValue, breakpoint;
    ImU32 lineNumber, currentLineNumber, currentLine;
    ImU32 keyword, declaration, number, string, identifier, knownIdentifier, comment;
    ImU32 headerName;
};

/// @brief System follows the desktop's light or dark setting.
enum class ThemeMode { System, Dark, Light };

/// @brief ThemeMode names in order, for settings and UI.
inline constexpr std::array themeModeNames = { "System", "Dark", "Light" };

/// @return The scheme in use.
const ColorScheme &currentScheme();

/// @brief Every scheme, dark and light; read it through colorSchemes(). Declared here so
///        theme.cpp's default scheme pointers into it are constant-initialized.
extern const std::array<ColorScheme, 6> colorSchemeTable;

/// @return All schemes, dark and light.
std::span<const ColorScheme> colorSchemes();

/// @return The named scheme, or nullptr.
const ColorScheme *findColorScheme( std::string_view name );

ThemeMode themeMode();

/// @brief Switches the mode and updates the ImGui style.
void setThemeMode( ThemeMode mode );

/// @brief Reads the desktop's light/dark setting for System mode; without an answer the last
///        one is kept (dark at first).
void followDesktopTheme();

const ColorScheme &darkColorScheme();

const ColorScheme &lightColorScheme();

/// @brief Makes the scheme the dark or light choice, by its dark flag.
/// @param[in] scheme An element of colorSchemes().
void setColorScheme( const ColorScheme &scheme );

/// @param[in] alpha 0 to 255.
ImU32 withAlpha( ImU32 color, int alpha );

inline constexpr ImU32 transparentColor = IM_COL32( 0, 0, 0, 0 );

inline constexpr float panelRounding = 10.0f;
inline constexpr float panelGap = 6.0f;
inline constexpr float toolbarFramePaddingY = 9.0f;
inline constexpr float editorTabsGap = 6.0f;

/// @brief Applies theme sizes and scheme colours to the ImGui style.
void applyTheme();

/// @return The user's accent, else the scheme's: selected tabs, toggles, outlines.
ImU32 accentColor();

bool hasCustomAccent();

/// @brief Goes back to the scheme's accent.
void resetAccentColor();

/// @return Accent faintly mixed into the panel background, for selected and hovered tabs.
ImU32 accentFillColor();

/// @brief Sets an accent for every scheme; alpha is ignored.
void setAccentColor( ImU32 color );

/// @return Background of the paused line.
ImU32 pausedLineColor();

/// @return Editor syntax colours on the current panel background.
TextEditor::Palette editorPalette();
