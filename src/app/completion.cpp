#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <set>

#include <app/aarch64_words.h>
#include <app/completion.h>
#include <core/header_declarations.h>

namespace {

bool startsWithIgnoringCase( const std::string &word, const std::string &prefix ) {
    return word.size() >= prefix.size() &&
           std::equal( prefix.begin(), prefix.end(), word.begin(), []( char a, char b ) {
               return std::tolower( static_cast<unsigned char>( a ) ) ==
                      std::tolower( static_cast<unsigned char>( b ) );
           } );
}

/// @param[in] convert std::toupper or std::tolower.
std::string withCase( std::string word, int ( *convert )( int ) ) {
    for ( char &c : word ) {
        c = static_cast<char>( convert( static_cast<unsigned char>( c ) ) );
    }
    return word;
}

/// @brief Finds completions of a term, shortest first. Without case sensitivity an
///        upper-case term gets upper-case language words, and document words differing only
///        in case from a language word are dropped.
/// @param[in] languageWords Lower case when the language ignores case.
std::vector<std::string> completeWord( const std::string &term,
                                       const std::set<std::string> &languageWords,
                                       const std::vector<std::string> &documentWords,
                                       bool caseSensitive ) {
    bool const upper = !caseSensitive && term == withCase( term, std::toupper ) &&
                       term != withCase( term, std::tolower );
    std::set<std::string> words;
    for ( const std::string &word : languageWords ) {
        words.insert( upper ? withCase( word, std::toupper ) : word );
    }
    for ( const std::string &word : documentWords ) {
        if ( !languageWords.contains( caseSensitive ? word : withCase( word, std::tolower ) ) ) {
            words.insert( word );
        }
    }

    std::vector<std::string> suggestions;
    for ( const std::string &word : words ) {
        if ( word.size() > term.size() && startsWithIgnoringCase( word, term ) ) {
            suggestions.push_back( word );
        }
    }
    std::ranges::stable_sort( suggestions, []( const std::string &a, const std::string &b ) {
        return a.size() < b.size();
    } );
    return suggestions;
}

/// @return AArch64 assembly words, lower case.
const std::set<std::string> &assemblyWords() {
    static const std::set<std::string> words = [] {
        std::set<std::string> words( aarch64Mnemonics.begin(), aarch64Mnemonics.end() );
        for ( const char *condition : aarch64Conditions ) {
            words.insert( std::string( "b." ) + condition );
        }
        words.insert( aarch64Conditions.begin(), aarch64Conditions.end() );
        words.insert( aarch64Directives.begin(), aarch64Directives.end() );
        words.insert( aarch64OperandKeywords.begin(), aarch64OperandKeywords.end() );
        words.insert( aarch64SystemRegisters.begin(), aarch64SystemRegisters.end() );
        words.insert( aarch64Registers().begin(), aarch64Registers().end() );
        return words;
    }();
    return words;
}

/// @brief C keywords, as in the editor's C language.
const std::array cKeywords = {
    "auto",       "break",     "case",           "char",
    "const",      "continue",  "default",        "do",
    "double",     "else",      "enum",           "extern",
    "float",      "for",       "goto",           "if",
    "inline",     "int",       "long",           "register",
    "restrict",   "return",    "short",          "signed",
    "sizeof",     "static",    "struct",         "switch",
    "typedef",    "union",     "unsigned",       "void",
    "volatile",   "while",     "_Alignas",       "_Alignof",
    "_Atomic",    "_Bool",     "_Complex",       "_Generic",
    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
};

/// @brief Directives, most used first, with the space before their argument.
const std::array cDirectives = {
    "include ", "define ",       "ifdef ",   "ifndef ",   "if ",    "else",
    "elif ",    "endif",         "undef ",   "pragma ",   "error ", "warning ",
    "line ",    "include_next ", "elifdef ", "elifndef ", "embed ",
};

/// @return Index of an #include line's '<' or '"', or nothing.
std::optional<size_t> headerNameStart( const std::string &line ) {
    size_t position = line.find_first_not_of( " \t" );
    if ( position == std::string::npos || line[position] != '#' ) {
        return std::nullopt;
    }
    position = line.find_first_not_of( " \t", position + 1 );
    for ( const char *directive : { "include_next", "include" } ) {
        if ( position != std::string::npos &&
             line.compare( position, std::strlen( directive ), directive ) == 0 ) {
            position = line.find_first_not_of( " \t", position + std::strlen( directive ) );
            if ( position != std::string::npos &&
                 ( line[position] == '<' || line[position] == '"' ) ) {
                return position;
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// @brief Lists .h files and folders matching a typed path in each search folder.
/// @param[in] typed Folder part picks the folder, last part starts the names.
/// @return Matching paths, folders ending in '/'.
std::set<std::string> headerPaths( const std::vector<std::string> &searchFolders,
                                   const std::string &typed ) {
    size_t const slash = typed.rfind( '/' );
    std::string const folder = slash == std::string::npos ? "" : typed.substr( 0, slash + 1 );
    std::string const namePrefix = typed.substr( folder.size() );
    std::set<std::string> paths;
    for ( const std::string &searchFolder : searchFolders ) {
        std::error_code error;
        for ( const auto &entry : std::filesystem::directory_iterator(
                  std::filesystem::path( searchFolder ) / folder, error ) ) {
            std::string const name = entry.path().filename();
            if ( !name.starts_with( namePrefix ) || name.starts_with( '.' ) ) {
                continue;
            }
            if ( entry.is_directory( error ) ) {
                paths.insert( folder + name + "/" );
            } else if ( entry.path().extension() == ".h" ) {
                paths.insert( folder + name );
            }
        }
    }
    return paths;
}

} // anonymous namespace

std::vector<std::string> completeAssembly( const std::string &term,
                                           const std::vector<std::string> &documentWords ) {
    return completeWord( term, assemblyWords(), documentWords, false );
}

std::vector<std::string> completeC( const std::string &term,
                                    const std::vector<Declaration> &declarations,
                                    const std::vector<std::string> &documentWords ) {
    std::set<std::string> words( std::begin( cKeywords ), std::end( cKeywords ) );
    for ( const Declaration &declaration : declarations ) {
        words.insert( declaration.name );
    }
    return completeWord( term, words, documentWords, true );
}

std::optional<IncludeCompletion> completeInclude( const std::string &line, size_t column,
                                                  const std::string &directory ) {
    std::optional<size_t> open = headerNameStart( line );
    if ( !open || column <= *open || column > line.size() ) {
        return std::nullopt;
    }
    char const close = line[*open] == '<' ? '>' : '"';
    std::string const typed = line.substr( *open + 1, column - *open - 1 );
    if ( typed.contains( close ) ) {
        return std::nullopt;
    }
    const std::vector<std::string> &searchFolders =
        close == '>' ? systemIncludeDirectories() : std::vector<std::string>{ directory };
    std::set<std::string> const paths = headerPaths( searchFolders, typed );

    IncludeCompletion completion;
    completion.start = *open + 1;
    completion.end = std::min( line.find( close, column ), line.size() );
    completion.names.assign( paths.begin(), paths.end() );
    std::ranges::stable_sort( completion.names, []( const std::string &a, const std::string &b ) {
        return a.size() < b.size();
    } );
    return completion;
}

std::optional<std::vector<std::string>> completeDirective( const std::string &line,
                                                           size_t column ) {
    size_t const hash = line.find_first_not_of( " \t" );
    if ( hash == std::string::npos || line[hash] != '#' || column <= hash ||
         column > line.size() ) {
        return std::nullopt;
    }
    size_t const wordStart = std::min( line.find_first_not_of( " \t", hash + 1 ), column );
    std::string typed = line.substr( wordStart, column - wordStart );
    if ( !std::ranges::all_of( typed, []( char c ) { return std::islower( c ) || c == '_'; } ) ) {
        return std::nullopt;
    }
    std::vector<std::string> directives;
    for ( const char *directive : cDirectives ) {
        std::string word = directive;
        bool const complete = word.size() > typed.size() && word[typed.size()] == ' ';
        if ( word.size() > typed.size() && word.starts_with( typed ) && !complete ) {
            directives.push_back( word );
        }
    }
    return directives;
}
