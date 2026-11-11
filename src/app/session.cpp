#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <app/session.h>

static const size_t maxRecentFiles = 10;
static const uint64_t emulatorSliceMicroseconds = 10000;

bool Session::openFile( const std::string &path, std::string &text ) {
    if ( !std::filesystem::exists( path ) ) {
        std::erase( recentFiles, path );
        status = "File no longer exists: " + path;
        return false;
    }

    std::ifstream file( path );
    if ( !file ) {
        status = "Cannot open " + path;
        return false;
    }

    std::stringstream content;
    content << file.rdbuf();
    text = content.str();
    filePath = path;
    setLineMarks( {} );
    status = "Opened " + path;
    addRecentFile( path );
    return true;
}

bool Session::saveFile( const std::string &path, const std::string &text ) {
    std::ofstream file( path );
    file << text;
    if ( !file ) {
        status = "Cannot save " + path;
        return false;
    }

    filePath = path;
    status = "Saved " + path;
    addRecentFile( path );
    return true;
}

bool Session::build() {
    stopActiveProgram();
    console.clear();
    executablePath.clear();

    BuildResult build = buildProgram( filePath );
    std::vector<LineMark> marks;
    for ( const Message &message : build.messages ) {
        console.push_back( message );
        if ( message.sourceLine > 0 ) {
            bool isWarning = message.kind == MessageKind::Warning;
            LineMarkKind kind = isWarning ? LineMarkKind::Warning : LineMarkKind::Error;
            marks.push_back( { message.sourceLine, kind, message.text } );
        }
    }
    setLineMarks( marks );

    if ( build.success ) {
        executablePath = build.executablePath;
        console.push_back( { "Build succeeded: " + executablePath, MessageKind::Success } );
        status = "Build succeeded";
    } else {
        console.push_back( { "Build failed", MessageKind::Error } );
        status = "Build failed";
    }
    return build.success;
}

void Session::run() {
    if ( !build() ) {
        return;
    }

    console.push_back( { "$ qemu-aarch64 " + executablePath, MessageKind::Command } );
    runningProgram = startProcess( { "qemu-aarch64", executablePath } );
    if ( !isRunning( runningProgram ) ) {
        printResult( "Cannot start qemu-aarch64", MessageKind::Error );
        return;
    }
    programLineOpen = false;
    status = "Running " + executablePath;
}

void Session::debugContinue() {
    if ( !isDebugging() ) {
        startDebugging( false );
    } else if ( isPaused() ) {
        continueEmulator( emulator );
        pausedLine = 0;
        setLineMarks( {} );
        status = "Running in debugger";
    }
}

void Session::debugStep() {
    if ( !isDebugging() ) {
        startDebugging( true );
    } else if ( isPaused() ) {
        stepEmulator( emulator );
        handleEmulatorState();
    }
}

void Session::stop() {
    if ( !isProgramActive() ) {
        return;
    }
    stopActiveProgram();
    printResult( "Program stopped", MessageKind::Warning );
}

void Session::toggleBreakpoint( int line ) {
    if ( breakpoints.contains( line ) ) {
        breakpoints.erase( line );
    } else {
        breakpoints.insert( line );
    }
    if ( isDebugging() ) {
        setBreakpointLines( emulator, breakpoints );
    }
}

void Session::update() {
    updateRunningProgram();
    updateEmulator();
}

bool Session::isDebugging() const {
    return emulator.engine != nullptr;
}

bool Session::isPaused() const {
    return emulator.state == EmulatorState::Paused;
}

bool Session::isProgramActive() const {
    return isRunning( runningProgram ) || isDebugging();
}

bool Session::isProgramBusy() const {
    return isRunning( runningProgram ) || emulator.state == EmulatorState::Running;
}

bool Session::readMemory( uint64_t address, uint8_t &byte ) const {
    return isPaused() && readMemoryByte( emulator, address, byte );
}

void Session::addRecentFile( const std::string &path ) {
    std::string absolutePath = std::filesystem::absolute( path ).lexically_normal();
    std::erase( recentFiles, absolutePath );
    recentFiles.insert( recentFiles.begin(), absolutePath );
    if ( recentFiles.size() > maxRecentFiles ) {
        recentFiles.resize( maxRecentFiles );
    }
}

void Session::setLineMarks( const std::vector<LineMark> &marks ) {
    lineMarks = marks;
    lineMarksVersion++;
}

void Session::appendProgramOutput( const std::string &output ) {
    for ( char c : output ) {
        if ( c == '\n' ) {
            programLineOpen = false;
            continue;
        }
        if ( !programLineOpen ) {
            console.push_back( {} );
            programLineOpen = true;
        }
        console.back().text += c;
    }
}

void Session::printResult( const std::string &text, MessageKind kind, int sourceLine ) {
    programLineOpen = false;
    console.push_back( { text, kind, sourceLine } );
    status = text;
}

void Session::printExitCode( int exitCode ) {
    std::string text = "Program exited with code " + std::to_string( exitCode );
    printResult( text, exitCode == 0 ? MessageKind::Success : MessageKind::Error );
}

void Session::stopActiveProgram() {
    if ( isRunning( runningProgram ) ) {
        stopProcess( runningProgram );
    }
    if ( isDebugging() ) {
        endDebugSession();
    }
}

void Session::startDebugging( bool pauseAtEntry ) {
    if ( !build() ) {
        return;
    }

    console.push_back( { "$ debug " + executablePath, MessageKind::Command } );
    if ( !loadProgram( emulator, executablePath ) ) {
        printResult( emulator.error, MessageKind::Error );
        return;
    }
    setBreakpointLines( emulator, breakpoints );
    programLineOpen = false;
    registers = readRegisters( emulator );
    hasRegisters = true;

    if ( pauseAtEntry ) {
        showPausedLine();
    } else {
        emulator.state = EmulatorState::Running;
        status = "Running in debugger";
    }
}

void Session::endDebugSession() {
    closeEmulator( emulator );
    emulator.state = EmulatorState::Idle;
    pausedLine = 0;
    setLineMarks( {} );
}

void Session::handleEmulatorState() {
    appendProgramOutput( emulator.output );
    emulator.output.clear();

    if ( emulator.state == EmulatorState::Paused ) {
        showPausedLine();
    } else if ( emulator.state == EmulatorState::Exited ) {
        int exitCode = emulator.exitCode;
        endDebugSession();
        printExitCode( exitCode );
    } else if ( emulator.state == EmulatorState::Crashed ) {
        showCrash();
    }
}

void Session::showPausedLine() {
    previousRegisters = registers;
    registers = readRegisters( emulator );
    pausedLine = sourceLineAt( emulator, programCounter( emulator ) );
    if ( pausedLine > 0 ) {
        setLineMarks( { { pausedLine, LineMarkKind::Paused, "" } } );
    } else {
        setLineMarks( {} );
    }
    lineToReveal = pausedLine;
    status = "Paused at line " + std::to_string( pausedLine );
}

void Session::showCrash() {
    int line = sourceLineAt( emulator, programCounter( emulator ) );
    std::string error = emulator.error;
    endDebugSession();

    std::string location = line > 0 ? " at line " + std::to_string( line ) : "";
    printResult( "Program crashed" + location + ": " + error, MessageKind::Error, line );
    if ( line > 0 ) {
        setLineMarks( { { line, LineMarkKind::Error, error } } );
    }
}

void Session::updateRunningProgram() {
    if ( !isRunning( runningProgram ) ) {
        return;
    }

    appendProgramOutput( pollProcess( runningProgram ) );
    if ( isRunning( runningProgram ) ) {
        return;
    }

    if ( runningProgram.signal != 0 ) {
        std::string signalName = strsignal( runningProgram.signal );
        printResult( "Program crashed: " + signalName, MessageKind::Error );
    } else {
        printExitCode( runningProgram.exitCode );
    }
}

void Session::updateEmulator() {
    if ( emulator.state != EmulatorState::Running ) {
        return;
    }
    runEmulator( emulator, emulatorSliceMicroseconds );
    handleEmulatorState();
}
