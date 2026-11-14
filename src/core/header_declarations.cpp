#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unistd.h>

#include <core/header_declarations.h>
#include <core/process.h>
#include <core/toolchain.h>

namespace {

/// @brief Reads a gcc -aux-info line: "/* stdio.h:370:NC */ extern int printf (...);".
/// @return True if it declares a function.
bool parseFunction( const std::string &line, Declaration &declaration ) {
    const char *marker = "*/ extern ";
    size_t markerStart = line.find( marker );
    if ( markerStart == std::string::npos ) {
        return false;
    }
    size_t headStart = markerStart + std::strlen( marker );
    size_t open = line.find( " (", headStart );
    size_t close = line.rfind( ')' );
    if ( open == std::string::npos || close == std::string::npos || close < open ) {
        return false;
    }
    std::string head = line.substr( headStart, open - headStart );
    size_t nameStart = head.find_last_of( " *" ) + 1;
    std::string returnType = head.substr( 0, nameStart );
    while ( returnType.ends_with( ' ' ) ) {
        returnType.pop_back();
    }
    declaration.name = head.substr( nameStart );
    declaration.detail = returnType + " " + line.substr( open + 1, close - open );
    return true;
}

/// @brief Reads a gcc -dM line: "#define SEEK_SET 0".
/// @return True if it defines a macro.
bool parseMacro( const std::string &line, Declaration &declaration ) {
    const char *marker = "#define ";
    if ( !line.starts_with( marker ) ) {
        return false;
    }
    size_t nameStart = std::strlen( marker );
    size_t nameEnd = std::min( line.find_first_of( " (", nameStart ), line.size() );
    declaration.name = line.substr( nameStart, nameEnd - nameStart );
    declaration.detail = line.substr( nameEnd );
    declaration.detail.erase( 0, declaration.detail.find_first_not_of( ' ' ) );
    return true;
}

} // anonymous namespace

std::string preprocessorLines( const std::string &text ) {
    std::istringstream input( text );
    std::string directives;
    std::string line;
    bool continued = false;
    while ( std::getline( input, line ) ) {
        size_t start = line.find_first_not_of( " \t" );
        if ( continued || ( start != std::string::npos && line[start] == '#' ) ) {
            directives += line + '\n';
            continued = line.ends_with( '\\' );
        }
    }
    return directives;
}

/// Functions from gcc -aux-info, macros from gcc -E -dM; each name listed once.
std::vector<Declaration> headerDeclarations( const std::string &text,
                                             const std::string &directory ) {
    std::filesystem::path temporary = std::filesystem::temp_directory_path() /
                                      ( "arm_ide_headers_" + std::to_string( getpid() ) );
    std::string source = temporary.string() + ".c";
    std::string functionsFile = temporary.string() + ".aux";
    std::ofstream( source ) << preprocessorLines( text );

    std::string compiler = crossTool( "gcc" );
    runCommand(
        { compiler, "-fsyntax-only", "-iquote", directory, "-aux-info", functionsFile, source },
        directory );
    CommandResult macros =
        runCommand( { compiler, "-E", "-dM", "-iquote", directory, source }, directory );

    std::vector<Declaration> declarations;
    std::set<std::string> names;
    auto add = [&]( const Declaration &declaration ) {
        if ( !declaration.name.starts_with( '_' ) && names.insert( declaration.name ).second ) {
            declarations.push_back( declaration );
        }
    };
    std::ifstream functionLines( functionsFile );
    std::istringstream macroLines( macros.output );
    std::string line;
    Declaration declaration;
    while ( std::getline( functionLines, line ) ) {
        if ( parseFunction( line, declaration ) ) {
            add( declaration );
        }
    }
    while ( std::getline( macroLines, line ) ) {
        if ( parseMacro( line, declaration ) ) {
            add( declaration );
        }
    }

    std::error_code error;
    std::filesystem::remove( source, error );
    std::filesystem::remove( functionsFile, error );
    return declarations;
}

/// gcc -v lists them between "search starts here:" and "End of search list.".
const std::vector<std::string> &systemIncludeDirectories() {
    static const std::vector<std::string> directories = [] {
        CommandResult command = runCommand(
            { crossTool( "gcc" ), "-E", "-v", "-x", "c", "/dev/null", "-o", "/dev/null" }, "/" );
        std::istringstream output( command.output );
        std::vector<std::string> found;
        std::string line;
        bool inList = false;
        while ( std::getline( output, line ) ) {
            if ( line.starts_with( "#include <...> search starts here:" ) ) {
                inList = true;
            } else if ( line.starts_with( "End of search list." ) ) {
                break;
            } else if ( inList && line.starts_with( ' ' ) ) {
                found.push_back( std::filesystem::path( line.substr( 1 ) ).lexically_normal() );
            }
        }
        return found;
    }();
    return directories;
}
