#include <language/aarch64_language.h>
#include <language/aarch64_words.h>
#include <string>

static bool isDigit( ImWchar c ) {
    return c >= '0' && c <= '9';
}

static bool isHexDigit( ImWchar c ) {
    return isDigit( c ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
}

static bool isIdentifierStart( ImWchar c ) {
    return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_' || c == '.' || c == '$';
}

static bool isIdentifierChar( ImWchar c ) {
    return isIdentifierStart( c ) || isDigit( c );
}

static TextEditor::Iterator getIdentifier( TextEditor::Iterator start, TextEditor::Iterator end ) {
    if ( start == end || !isIdentifierStart( *start ) ) {
        return start;
    }

    TextEditor::Iterator i = start;
    while ( i < end && isIdentifierChar( *i ) ) {
        i++;
    }
    return i;
}

static TextEditor::Iterator getNumber( TextEditor::Iterator start, TextEditor::Iterator end ) {
    if ( start == end || !isDigit( *start ) ) {
        return start;
    }

    TextEditor::Iterator i = start;
    i++;
    bool isHex = *start == '0' && i < end && ( *i == 'x' || *i == 'X' );
    if ( isHex ) {
        i++;
        while ( i < end && isHexDigit( *i ) ) {
            i++;
        }
    } else {
        while ( i < end && isDigit( *i ) ) {
            i++;
        }
    }
    return i;
}

const TextEditor::Language *aarch64Language() {
    static TextEditor::Language language;
    if ( !language.name.empty() ) {
        return &language;
    }

    language.name = "AArch64";
    language.caseSensitive = false;
    language.singleLineComment = "//";
    language.commentStart = "/*";
    language.commentEnd = "*/";
    language.hasDoubleQuotedStrings = true;
    language.hasSingleQuotedStrings = true;
    language.stringEscape = '\\';
    language.getIdentifier = getIdentifier;
    language.getNumber = getNumber;
    language.isPunctuation = []( ImWchar c ) {
        return c == '[' || c == ']' || c == '{' || c == '}' || c == ',' || c == '!' || c == '#' ||
               c == ':' || c == '+' || c == '-' || c == '=' || c == '(' || c == ')';
    };

    language.keywords.insert( aarch64Mnemonics.begin(), aarch64Mnemonics.end() );
    for ( const char *condition : aarch64Conditions ) {
        language.keywords.insert( std::string( "b." ) + condition );
    }
    language.declarations.insert( aarch64Directives.begin(), aarch64Directives.end() );

    language.identifiers.insert( aarch64Conditions.begin(), aarch64Conditions.end() );
    language.identifiers.insert( aarch64OperandKeywords.begin(), aarch64OperandKeywords.end() );
    language.identifiers.insert( aarch64SystemRegisters.begin(), aarch64SystemRegisters.end() );
    for ( int i = 0; i <= 31; i++ ) {
        std::string number = std::to_string( i );
        for ( const char *prefix : { "v", "q", "d", "s", "h", "b" } ) {
            language.identifiers.insert( prefix + number );
        }
        if ( i <= 30 ) {
            language.identifiers.insert( "x" + number );
            language.identifiers.insert( "w" + number );
        }
    }
    language.identifiers.insert( { "sp", "wsp", "xzr", "wzr", "lr", "fp" } );

    return &language;
}
