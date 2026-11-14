#pragma once

#include <optional>
#include <string>
#include <vector>

/// @brief A font file and the face inside it, as FreeType opens it.
struct FontFace {
    std::string path;
    int index = 0;
};

/// @brief Lists the installed font families that cover English, without colour (emoji) fonts,
///        through fontconfig (fc-list).
/// @param[in] monospaceOnly True to list only fixed-width families.
/// @return Family names, sorted and without repeats; empty if fc-list is missing.
std::vector<std::string> fontFamilies( bool monospaceOnly );

/// @brief Finds the regular face of an installed family through fontconfig (fc-match).
/// @param[in] family Family name, compared ignoring case.
/// @return Its face, or nothing if the family is not installed (fontconfig would substitute
///         another one).
std::optional<FontFace> findFont( const std::string &family );

/// @brief Loads the interface and code fonts chosen in the settings, or else the first
///        recommended one installed, and sets the interface font size.
void loadFonts();

/// @brief Loads a font family asked for with setUiFontFamily() or setCodeFontFamily().
///
/// Runs between frames: the old font can only be removed when no draw data uses it.
void applyRequestedFonts();

/// @brief Pushes the code font at the current zoom. Undo with ImGui::PopFont.
void pushCodeFont();

/// @brief Changes the code font size, within limits.
/// @param[in] step Points to add; negative to zoom out.
void zoomCode( float step );

/// @brief Restores the default code font size.
void resetZoom();

/// @return Code font size in points.
float codeFontSize();

/// @brief Sets the code font size, clamped to its limits, without marking the settings for
///        saving.
/// @param[in] size New size in points.
void setCodeFontSize( float size );

/// @return Family of the interface font; empty for ImGui's built-in font.
const std::string &uiFontFamily();

/// @return Family of the code font; empty for ImGui's built-in font.
const std::string &codeFontFamily();

/// @brief Switches the interface font at the next frame and saves it in the settings. The
///        current font is kept if the family can't be loaded.
/// @param[in] family Installed family, as fontFamilies() lists it.
void setUiFontFamily( const std::string &family );

/// @brief Switches the code font at the next frame and saves it in the settings. The current
///        font is kept if the family can't be loaded.
/// @param[in] family Installed family, as fontFamilies() lists it.
void setCodeFontFamily( const std::string &family );
