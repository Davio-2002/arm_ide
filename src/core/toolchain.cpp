#include "core/toolchain.h"

#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>

#include "core/process.h"

static std::string commandLine( const std::vector<std::string> &args ) {
    std::string line = "$";
    for ( const std::string &arg : args ) {
        line += " " + arg;
    }
    return line;
}

static Message parseToolOutputLine( const std::string &text ) {
    static const std::regex sourceLocation( "^[^:]+:([0-9]+):" );

    Message message;
    message.text = text;
    std::smatch match;
    if ( std::regex_search( text, match, sourceLocation ) ) {
        message.sourceLine = std::stoi( match[1] );
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

    std::vector<std::vector<std::string>> steps = {
        { "aarch64-linux-gnu-as", "-g", source, "-o", objectPath },
        { "aarch64-linux-gnu-ld", objectPath, "-o", executablePath },
    };

    for ( const std::vector<std::string> &step : steps ) {
        build.messages.push_back( { commandLine( step ), MessageKind::Command } );
        CommandResult command = runCommand( step, source.parent_path() );

        std::istringstream output( command.output );
        std::string line;
        while ( std::getline( output, line ) ) {
            build.messages.push_back( parseToolOutputLine( line ) );
        }
        if ( command.exitCode != 0 ) {
            return build;
        }
    }

    build.success = true;
    build.executablePath = executablePath;
    return build;
}

std::map<uint64_t, int> readSourceLines( const std::string &executablePath ) {
    std::string directory = std::filesystem::path( executablePath ).parent_path();
    CommandResult command = runCommand(
        { "aarch64-linux-gnu-objdump", "--dwarf=decodedline", executablePath }, directory );

    std::map<uint64_t, int> sourceLines;
    std::istringstream output( command.output );
    std::string row;
    while ( std::getline( output, row ) ) {
        std::istringstream columns( row );
        std::string fileName, line, address;
        columns >> fileName >> line >> address;
        bool isLineRow = !line.empty() && std::isdigit( line[0] ) && address.starts_with( "0x" );
        if ( isLineRow ) {
            sourceLines[std::stoull( address, nullptr, 16 )] = std::stoi( line );
        }
    }
    return sourceLines;
}
