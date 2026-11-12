#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>

#include <core/process.h>
#include <core/toolchain.h>

std::string crossTool( const std::string &name ) {
    static const char *const prefixes[] = {
        "aarch64-linux-gnu-",      "aarch64-unknown-linux-gnu-", "aarch64-suse-linux-",
        "aarch64-none-linux-gnu-", "aarch64-linux-musl-",        "aarch64-alpine-linux-musl-",
    };
    for ( const char *prefix : prefixes ) {
        if ( isInPath( std::string( prefix ) + "as" ) ) {
            return prefix + name;
        }
    }
    return prefixes[0] + name;
}

std::string qemuProgram() {
    for ( const char *program : { "qemu-aarch64", "qemu-aarch64-static" } ) {
        if ( isInPath( program ) ) {
            return program;
        }
    }
    return "";
}

static std::string commandLine( const std::vector<std::string> &args ) {
    std::string line = "$";
    for ( const std::string &arg : args ) {
        line += " " + arg;
    }
    return line;
}

// Only locations in the source itself get a line, not those in headers it includes.
static Message parseToolOutputLine( const std::string &text, const std::string &sourceName ) {
    static const std::regex sourceLocation( "^([^:]+):([0-9]+):" );

    Message message;
    message.text = text;
    std::smatch match;
    if ( std::regex_search( text, match, sourceLocation ) &&
         std::filesystem::path( match[1].str() ).filename() == sourceName ) {
        message.sourceLine = std::stoi( match[2] );
    }

    bool isError = text.contains( "Error" ) || text.contains( "error" ) ||
                   text.contains( "undefined reference" );
    bool isWarning = text.contains( "Warning" ) || text.contains( "warning" );
    if ( isError ) {
        message.kind = MessageKind::Error;
    } else if ( isWarning ) {
        message.kind = MessageKind::Warning;
    }
    return message;
}

BuildResult buildProgram( const std::string &sourcePath ) {
    BuildResult build;
    std::error_code error;
    std::filesystem::path source = std::filesystem::absolute( sourcePath, error );
    if ( error ) {
        build.messages.push_back( { "Cannot resolve " + sourcePath, MessageKind::Error } );
        return build;
    }
    if ( !source.has_extension() ) {
        build.messages.push_back(
            { "Source file needs an extension, for example program.s", MessageKind::Error } );
        return build;
    }

    std::string objectPath = std::filesystem::path( source ).replace_extension( ".o" );
    std::string executablePath = std::filesystem::path( source ).replace_extension( "" );

    // C links glibc statically, so QEMU needs no AArch64 libraries on the host.
    std::vector<std::vector<std::string>> steps;
    if ( source.extension() == ".c" ) {
        steps = { { crossTool( "gcc" ), "-g", "-O0", "-static", source, "-o", executablePath } };
    } else {
        steps = { { crossTool( "as" ), "-g", source, "-o", objectPath },
                  { crossTool( "ld" ), objectPath, "-o", executablePath } };
    }

    for ( const std::vector<std::string> &step : steps ) {
        build.messages.push_back( { commandLine( step ), MessageKind::Command } );
        if ( !isInPath( step[0] ) ) {
            bool isCompiler = source.extension() == ".c";
            build.messages.push_back(
                { step[0] + " not found in PATH: install the AArch64 " +
                      ( isCompiler ? "GCC cross compiler with its C library" : "binutils" ),
                  MessageKind::Error } );
            return build;
        }
        CommandResult command = runCommand( step, source.parent_path() );

        std::istringstream output( command.output );
        std::string line;
        while ( std::getline( output, line ) ) {
            build.messages.push_back( parseToolOutputLine( line, source.filename() ) );
        }
        if ( command.exitCode != 0 ) {
            return build;
        }
    }

    build.success = true;
    build.executablePath = executablePath;
    return build;
}

// Rows of other files and ends of sequences get line 0, so code without lines of the source
// (glibc) is not taken for its last line.
std::map<uint64_t, int> readSourceLines( const std::string &executablePath,
                                         const std::string &sourcePath ) {
    static const std::regex lineRow( "^(.*\\S)\\s+([0-9]+|-)\\s+0x([0-9a-f]+)" );
    std::string directory = std::filesystem::path( executablePath ).parent_path();
    CommandResult command = runCommand(
        { crossTool( "objdump" ), "--dwarf=decodedline", "--wide", executablePath }, directory );

    std::string sourceName = std::filesystem::path( sourcePath ).filename();
    std::map<uint64_t, int> sourceLines;
    std::istringstream output( command.output );
    std::string row;
    std::smatch match;
    while ( std::getline( output, row ) ) {
        if ( !std::regex_search( row, match, lineRow ) ) {
            continue;
        }
        bool isSourceRow = match[1] == sourceName && match[2] != "-";
        sourceLines[std::stoull( match[3], nullptr, 16 )] = isSourceRow ? std::stoi( match[2] ) : 0;
    }
    return sourceLines;
}

uint64_t symbolAddress( const std::string &executablePath, const std::string &name ) {
    std::string directory = std::filesystem::path( executablePath ).parent_path();
    CommandResult command =
        runCommand( { crossTool( "nm" ), "--defined-only", executablePath }, directory );
    std::istringstream output( command.output );
    std::string address, type, symbol;
    while ( output >> address >> type >> symbol ) {
        if ( symbol == name ) {
            return std::stoull( address, nullptr, 16 );
        }
    }
    return 0;
}
