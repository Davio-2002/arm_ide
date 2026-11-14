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
