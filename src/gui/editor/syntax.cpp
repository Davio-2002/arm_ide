#include <array>
#include <iterator>
#include <string>
#include <utility>

#include <app/language/aarch64_words.h>
#include <gui/editor/syntax.h>

namespace {

bool isDigit( ImWchar c ) {
    return c >= '0' && c <= '9';
}

bool isHexDigit( ImWchar c ) {
    return isDigit( c ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
}

/// @return True for a letter, '_', '.' or '$'.
bool isIdentifierStart( ImWchar c ) {
    return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_' || c == '.' || c == '$';
}

bool isIdentifierChar( ImWchar c ) {
    return isIdentifierStart( c ) || isDigit( c );
}

/// @brief Tokenizer callback: end of a word such as b.eq or .global.
/// @return End of the word, or `start` if none starts there.
TextEditor::Iterator getIdentifier( TextEditor::Iterator start, TextEditor::Iterator end ) {
    if ( start == end || !isIdentifierStart( *start ) ) {
        return start;
    }

    TextEditor::Iterator i = start;
    while ( i < end && isIdentifierChar( *i ) ) {
        i++;
    }
    return i;
}

/// @brief Tokenizer callback: end of a decimal or 0x number.
/// @return End of the number, or `start` if none starts there.
TextEditor::Iterator getNumber( TextEditor::Iterator start, TextEditor::Iterator end ) {
    if ( start == end || !isDigit( *start ) ) {
        return start;
    }

    TextEditor::Iterator i = start;
    i++;
    bool const isHex = *start == '0' && i < end && ( *i == 'x' || *i == 'X' );
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

const std::array libraryNames = {
    "printf",
    "fprintf",
    "sprintf",
    "snprintf",
    "scanf",
    "fscanf",
    "sscanf",
    "puts",
    "fputs",
    "putchar",
    "fputc",
    "getchar",
    "fgetc",
    "fgets",
    "fopen",
    "fclose",
    "fread",
    "fwrite",
    "fflush",
    "fseek",
    "ftell",
    "rewind",
    "feof",
    "ferror",
    "perror",
    "remove",
    "rename",
    "FILE",
    "EOF",
    "stdin",
    "stdout",
    "stderr",
    "SEEK_SET",
    "SEEK_CUR",
    "SEEK_END",
    "BUFSIZ",
    "malloc",
    "calloc",
    "realloc",
    "free",
    "exit",
    "abort",
    "atexit",
    "atoi",
    "atol",
    "atoll",
    "atof",
    "strtol",
    "strtoul",
    "strtoll",
    "strtoull",
    "strtod",
    "abs",
    "labs",
    "llabs",
    "div",
    "rand",
    "srand",
    "qsort",
    "bsearch",
    "getenv",
    "system",
    "EXIT_SUCCESS",
    "EXIT_FAILURE",
    "RAND_MAX",
    "NULL",
    "strlen",
    "strcpy",
    "strncpy",
    "strcat",
    "strncat",
    "strcmp",
    "strncmp",
    "strchr",
    "strrchr",
    "strstr",
    "strtok",
    "strerror",
    "memcpy",
    "memmove",
    "memset",
    "memcmp",
    "memchr",
    "isalpha",
    "isdigit",
    "isalnum",
    "isspace",
    "isupper",
    "islower",
    "isxdigit",
    "ispunct",
    "isprint",
    "toupper",
    "tolower",
    "sqrt",
    "pow",
    "fabs",
    "floor",
    "ceil",
    "round",
    "sin",
    "cos",
    "tan",
    "exp",
    "log",
    "log10",
    "fmod",
    "size_t",
    "ptrdiff_t",
    "offsetof",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "intptr_t",
    "uintptr_t",
    "bool",
    "true",
    "false",
    "INT_MIN",
    "INT_MAX",
    "UINT_MAX",
    "LONG_MIN",
    "LONG_MAX",
    "CHAR_BIT",
    "SIZE_MAX",
    "assert",
    "errno",
    "time",
    "clock",
    "time_t",
    "clock_t",
    "CLOCKS_PER_SEC",
    "sleep",
    "read",
    "write",
    "getpid",
    "main",
};

/// The next token after a coloured #include is its header name.
bool headerNameExpected = false;

/// @brief Colours directive words, the header name after #include, and strings. The editor
///        would colour a whole preprocessor line alike and a quoted header as a string.
/// @param[out] color Token colour.
/// @return End of the token, or `start` if it is none of these.
TextEditor::Iterator tokenizeC( TextEditor::Iterator start, TextEditor::Iterator end,
                                TextEditor::Color &color ) {
    bool const isHeaderName = std::exchange( headerNameExpected, false ) && start.hasPrevious();
    TextEditor::Iterator token = start;
    if ( *start == '#' ) {
        token++;
        while ( token < end && ( *token == ' ' || *token == '\t' ) ) {
            token++;
        }
        std::string directive;
        while ( token < end && ( ( *token >= 'a' && *token <= 'z' ) || *token == '_' ) ) {
            directive += static_cast<char>( *token++ );
        }
        headerNameExpected = directive == "include" || directive == "include_next";
        color = TextEditor::Color::preprocessor;
        return token;
    }
    if ( *start == '"' || ( isHeaderName && *start == '<' ) ) {
        ImWchar const close = *start == '<' ? u'>' : u'"';
        token++;
        while ( token < end && *token != close ) {
            if ( *token == '\\' && close == '"' ) {
                token++;
            }
            if ( token < end ) {
                token++;
            }
        }
        if ( token < end ) {
            token++;
        }
        color = isHeaderName ? TextEditor::Color::customColor1 : TextEditor::Color::string;
        return token;
    }
    return start;
}

} // anonymous namespace

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
    language.identifiers.insert( aarch64Registers().begin(), aarch64Registers().end() );

    return &language;
}

const TextEditor::Language *cLanguage() {
    static TextEditor::Language const language = [] {
        TextEditor::Language c = *TextEditor::LookupLanguageByExtension( ".c" );
        c.preprocess = 0;
        c.hasDoubleQuotedStrings = false;
        c.customTokenizer = tokenizeC;
        c.identifiers.insert( std::begin( libraryNames ), std::end( libraryNames ) );
        return c;
    }();
    return &language;
}
