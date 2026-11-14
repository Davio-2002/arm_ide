#include <algorithm>
#include <cctype>

#include <app/completion.h>

namespace {

/// @param[in] prefix Prefix to look for.
/// @return True if word starts with prefix, ignoring case.
bool startsWithIgnoringCase( const std::string &word, const std::string &prefix ) {
    return word.size() >= prefix.size() &&
           std::equal( prefix.begin(), prefix.end(), word.begin(), []( char a, char b ) {
               return std::tolower( static_cast<unsigned char>( a ) ) ==
                      std::tolower( static_cast<unsigned char>( b ) );
           } );
}

/// @param[in] convert std::toupper or std::tolower.
/// @return The word with each character converted.
std::string withCase( std::string word, int ( *convert )( int ) ) {
    for ( char &c : word ) {
        c = static_cast<char>( convert( static_cast<unsigned char>( c ) ) );
    }
    return word;
}

} // anonymous namespace

std::vector<std::string> completeWord( const std::string &term,
                                       const std::set<std::string> &languageWords,
                                       const std::vector<std::string> &documentWords,
                                       bool caseSensitive ) {
    bool upper = !caseSensitive && term == withCase( term, std::toupper ) &&
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
