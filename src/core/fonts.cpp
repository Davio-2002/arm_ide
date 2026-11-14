#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

#include <core/fonts.h>
#include <core/process.h>

namespace {

/// @param[in] text      Text to split.
/// @param[in] separator Field separator.
/// @return The fields, empty ones included.
std::vector<std::string> split( const std::string &text, char separator ) {
    std::vector<std::string> fields;
    std::istringstream stream( text );
    std::string field;
    while ( std::getline( stream, field, separator ) ) {
        fields.push_back( field );
    }
    return fields;
}

/// @return True if the two names are equal ignoring ASCII case.
bool sameName( const std::string &a, const std::string &b ) {
    return std::ranges::equal( a, b, []( unsigned char x, unsigned char y ) {
        return std::tolower( x ) == std::tolower( y );
    } );
}

/// @brief Tells whether ImGui's FreeType loader can use a font format: TrueType or CFF
///        (OpenType), not Type 1 or bitmap fonts.
bool isScalableFormat( const std::string &format ) {
    return format == "TrueType" || format == "CFF";
}

} // anonymous namespace

std::vector<std::string> fontFamilies( bool monospaceOnly ) {
    if ( !isInPath( "fc-list" ) ) {
        return {};
    }
    std::string pattern =
        monospaceOnly ? ":lang=en:color=false:spacing=mono" : ":lang=en:color=false";
    std::vector<std::string> args = { "fc-list", pattern, "--format",
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
    CommandResult result = runCommand( { "fc-match", family + ":style=Regular", "--format",
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
