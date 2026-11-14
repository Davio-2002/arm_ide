#pragma once

#include <optional>
#include <string>
#include <vector>

/// @brief A font file and the face index inside it.
struct FontFace {
    std::string path;
    int index = 0;
};

/// @brief Lists installed families covering English, without emoji fonts (fc-list).
/// @return Sorted unique names; empty if fc-list is missing.
std::vector<std::string> fontFamilies( bool monospaceOnly );

/// @brief Finds the regular face of a family (fc-match), ignoring case.
/// @return Nothing if not installed, since fontconfig would substitute another.
std::optional<FontFace> findFont( const std::string &family );

/// @brief Loads the saved UI and code fonts, else the first recommended one installed.
void loadFonts();

/// @brief Loads fonts asked by setUiFontFamily()/setCodeFontFamily(). Runs between frames,
///        as an old font can't be removed while draw data uses it.
void applyRequestedFonts();

/// @brief Pushes the code font at the current zoom; pop with ImGui::PopFont.
void pushCodeFont();

/// @param[in] step Points to add; negative zooms out.
void zoomCode( float step );

void resetZoom();

/// @return Code font size in points.
float codeFontSize();

/// @brief Sets the clamped size without marking the settings for saving.
void setCodeFontSize( float size );

/// @return Empty for ImGui's built-in font.
const std::string &uiFontFamily();

/// @return Empty for ImGui's built-in font.
const std::string &codeFontFamily();

/// @brief Switches the UI font next frame and saves it; kept if the family fails to load.
void setUiFontFamily( const std::string &family );

/// @brief Switches the code font next frame and saves it; kept if the family fails to load.
void setCodeFontFamily( const std::string &family );
