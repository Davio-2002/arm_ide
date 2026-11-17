#include <imgui.h>
#include <imgui_internal.h>
#include <misc/freetype/imgui_freetype.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <sstream>

#include <core/process.h>
#include <gui/fonts.h>

namespace {

const float uiFontSize = 22.0f;
const float defaultCodeFontSize = 18.0f;
const float minCodeFontSize = 10.0f;
const float maxCodeFontSize = 48.0f;

float currentCodeFontSize = defaultCodeFontSize;

/// @return Fields, empty ones included.
std::vector<std::string> split( const std::string &text, char separator ) {
    std::vector<std::string> fields;
    std::istringstream stream( text );
    std::string field;
    while ( std::getline( stream, field, separator ) ) {
        fields.push_back( field );
    }
    return fields;
}

/// @return True if equal ignoring ASCII case.
bool sameName( const std::string &a, const std::string &b ) {
    return std::ranges::equal( a, b, []( unsigned char x, unsigned char y ) {
        return std::tolower( x ) == std::tolower( y );
    } );
}

/// @return True for TrueType or CFF, which FreeType loads; not Type 1 or bitmap.
bool isScalableFormat( const std::string &format ) {
    return format == "TrueType" || format == "CFF";
}

struct LoadedFont {
    std::string family;
    std::string requestedFamily;
    ImFont *font = nullptr;
};

LoadedFont uiFont;
LoadedFont codeFont;

/// @brief Loads the first installed, readable candidate. If none is, keeps the font, or
///        uses ImGui's built-in one when there is none yet.
/// @param[in] candidates In order of preference; empty names skipped.
void loadFont( LoadedFont &loaded, const std::vector<std::string> &candidates ) {
    ImFontAtlas *atlas = ImGui::GetIO().Fonts;
    ImFont *font = nullptr;
    std::string family;
    for ( const std::string &candidate : candidates ) {
        std::optional<FontFace> face = findFont( candidate );
        if ( !face ) {
            continue;
        }
        ImFontConfig config;
        config.FontNo = face->index;
        config.Flags |= ImFontFlags_NoLoadError;
        font = atlas->AddFontFromFileTTF( face->path.c_str(), 0.0f, &config );
        if ( font ) {
            family = candidate;
            break;
        }
    }
    if ( !font && !loaded.font ) {
        font = atlas->AddFontDefault();
    }
    if ( !font ) {
        return;
    }
    if ( loaded.font ) {
        atlas->RemoveFont( loaded.font );
    }
    loaded.font = font;
    loaded.family = family;
}

/// @return `first`, then the recommended families.
std::vector<std::string> fontCandidates( const std::string &first,
                                         std::initializer_list<const char *> recommended ) {
    std::vector<std::string> candidates = { first };
    candidates.insert( candidates.end(), recommended.begin(), recommended.end() );
    return candidates;
}

/// @brief Loads the requested family between frames; the font is kept if it can't load.
void applyRequestedFont( LoadedFont &loaded ) {
    if ( loaded.requestedFamily.empty() ) {
        return;
    }
    loadFont( loaded, { loaded.requestedFamily } );
    loaded.requestedFamily.clear();
    ImGui::MarkIniSettingsDirty();
}

} // anonymous namespace

std::vector<std::string> fontFamilies( bool monospaceOnly ) {
    if ( !isInPath( "fc-list" ) ) {
        return {};
    }
    std::string const pattern =
        monospaceOnly ? ":lang=en:color=false:spacing=mono" : ":lang=en:color=false";
    std::vector<std::string> const args = { "fc-list", pattern, "--format",
                                            "%{fontformat}\t%{family[0]}\n" };
    std::vector<std::string> families;
    std::istringstream lines( runCommand( args, "" ).output );
    std::string line;
    while ( std::getline( lines, line ) ) {
        std::vector<std::string> fields = split( line, '\t' );
        if ( fields.size() == 2 && isScalableFormat( fields[0] ) && !fields[1].empty() &&
             !fields[1].starts_with( '.' ) ) {
            families.push_back( fields[1] );
        }
    }
    std::ranges::sort( families );
    auto repeats = std::ranges::unique( families );
    families.erase( repeats.begin(), repeats.end() );
    return families;
}

std::optional<FontFace> findFont( const std::string &family ) {
    if ( family.empty() || !isInPath( "fc-match" ) ) {
        return std::nullopt;
    }
    CommandResult const result = runCommand( { "fc-match", family + ":style=Regular", "--format",
                                               "%{fontformat}\t%{index}\t%{family}\t%{file}\n" },
                                             "" );
    std::istringstream lines( result.output );
    std::string line;
    while ( std::getline( lines, line ) ) {
        std::vector<std::string> fields = split( line, '\t' );
        if ( fields.size() != 4 || !isScalableFormat( fields[0] ) ) {
            continue;
        }
        for ( const std::string &name : split( fields[2], ',' ) ) {
            if ( sameName( name, family ) ) {
                return FontFace{ fields[3], std::atoi( fields[1].c_str() ) };
            }
        }
    }
    return std::nullopt;
}

void loadFonts() {
    ImGui::GetStyle().FontSizeBase = uiFontSize;
    ImGui::GetIO().Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    loadFont( uiFont, fontCandidates( uiFont.requestedFamily, { "Ubuntu Sans", "Inter", "Noto Sans",
                                                                "Cantarell", "DejaVu Sans" } ) );
    loadFont( codeFont, fontCandidates( codeFont.requestedFamily,
                                        { "JetBrains Mono", "JetBrainsMono Nerd Font", "Fira Code",
                                          "Cascadia Code", "Source Code Pro", "Ubuntu Mono",
                                          "DejaVu Sans Mono" } ) );
    uiFont.requestedFamily.clear();
    codeFont.requestedFamily.clear();
    ImGui::GetIO().FontDefault = uiFont.font;
}

void applyRequestedFonts() {
    applyRequestedFont( uiFont );
    applyRequestedFont( codeFont );
    ImGui::GetIO().FontDefault = uiFont.font;
}

const std::string &uiFontFamily() {
    return uiFont.family;
}

const std::string &codeFontFamily() {
    return codeFont.family;
}

void setUiFontFamily( const std::string &family ) {
    uiFont.requestedFamily = family;
}

void setCodeFontFamily( const std::string &family ) {
    codeFont.requestedFamily = family;
}

void pushCodeFont() {
    ImGui::PushFont( codeFont.font, currentCodeFontSize );
}

float codeFontSize() {
    return currentCodeFontSize;
}

void setCodeFontSize( float size ) {
    currentCodeFontSize = std::clamp( size, minCodeFontSize, maxCodeFontSize );
}

void zoomCode( float step ) {
    setCodeFontSize( currentCodeFontSize + step );
    ImGui::MarkIniSettingsDirty();
}

void resetZoom() {
    setCodeFontSize( defaultCodeFontSize );
    ImGui::MarkIniSettingsDirty();
}
