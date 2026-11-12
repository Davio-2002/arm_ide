#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <app/session.h>

static const size_t maxRecentFiles = 10;
static const int debuggerPollMilliseconds = 1;
static const uint64_t memoryBlockSize = 256;

static std::string absolutePath( const std::string &path ) {
    return std::filesystem::absolute( path ).lexically_normal();
}

bool Session::openFile( const std::string &path, std::string &text ) {
    if ( !std::filesystem::exists( path ) ) {
        status = "File no longer exists: " + path;
        std::erase( recentFiles, path );
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
    filePath = absolutePath( path );
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

    std::string savedPath = absolutePath( path );
    if ( savedPath != filePath ) {
        breakpoints[savedPath] = std::move( breakpoints[filePath] );
        breakpoints.erase( filePath );
    }
    filePath = savedPath;
    status = "Saved " + path;
    addRecentFile( path );
    return true;
}

bool Session::build() {
    stopActiveProgram();
    buildLog.clear();
    executablePath.clear();
    programFilePath = filePath;

    BuildResult build = buildProgram( filePath );
    std::vector<LineMark> marks;
    for ( const Message &message : build.messages ) {
        buildLog.push_back( message );
        if ( message.sourceLine > 0 ) {
            bool isWarning = message.kind == MessageKind::Warning;
            LineMarkKind kind = isWarning ? LineMarkKind::Warning : LineMarkKind::Error;
            marks.push_back( { message.sourceLine, kind, message.text } );
        }
    }
    setLineMarks( marks );

    if ( build.success ) {
        executablePath = build.executablePath;
        buildLog.push_back( { "Build succeeded: " + executablePath, MessageKind::Success } );
        status = "Build succeeded";
    } else {
        buildLog.push_back( { "Build failed", MessageKind::Error } );
        status = "Build failed";
    }
    buildCount++;
    buildSucceeded = build.success;
    return build.success;
}

void Session::run() {
    if ( !build() ) {
        return;
    }

    console.clear();
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
        continueDebugger( debugger );
        handleDebuggerState();
    }
}

void Session::debugStep() {
    if ( !isDebugging() ) {
        startDebugging( true );
    } else if ( isPaused() ) {
        stepDebugger( debugger );
        handleDebuggerState();
    }
}

void Session::stop() {
    if ( !isProgramActive() ) {
        return;
    }
    stopActiveProgram();
    printResult( "Program stopped", MessageKind::Warning );
}

void Session::toggleBreakpoint( const std::string &path, int line ) {
    std::set<int> &lines = breakpoints[path];
    if ( lines.contains( line ) ) {
        lines.erase( line );
    } else {
        lines.insert( line );
    }
    if ( isDebugging() && path == programFilePath ) {
        bool wasRunning = debugger.state == DebuggerState::Running;
        setBreakpointLines( debugger, lines );
        if ( wasRunning ) {
            handleDebuggerState();
        }
    }
}

Process *Session::programProcess() {
    if ( isRunning( runningProgram ) ) {
        return &runningProgram;
    }
    if ( isRunning( debugger.qemu ) ) {
        return &debugger.qemu;
    }
    return nullptr;
}

bool Session::acceptsInput() const {
    return isRunning( runningProgram ) || isRunning( debugger.qemu );
}

// The line is echoed into the console like a terminal does.
void Session::sendProgramInput( const std::string &line ) {
    Process *program = programProcess();
    if ( !program ) {
        return;
    }
    appendProgramOutput( line + "\n" );
    if ( !writeToProcess( *program, line + "\n" ) ) {
        status = "The program is not reading its input";
    }
}

void Session::endProgramInput() {
    if ( Process *program = programProcess() ) {
        endProcessInput( *program );
        status = "End of input sent";
    }
}

void Session::clearOutput() {
    console.clear();
    buildLog.clear();
    programLineOpen = false;
}

static std::string terminalStartDirectory( const std::string &filePath ) {
    std::string directory = std::filesystem::path( filePath ).parent_path();
    const char *home = std::getenv( "HOME" );
    return directory.empty() && home ? home : directory;
}

void Session::openTerminal( int rows, int columns ) {
    if ( !isRunning( terminal ) && !terminal.exited ) {
        startTerminal( terminal, terminalStartDirectory( filePath ), rows, columns );
        terminalActivity = std::chrono::steady_clock::now();
    } else if ( rows != terminal.rows || columns != terminal.columns ) {
        resizeTerminal( terminal, rows, columns );
    }
}

// After the shell exited, a key press starts a new one.
void Session::sendTerminalInput( const std::vector<TerminalInput> &inputs,
                                 const std::string &paste ) {
    if ( inputs.empty() && paste.empty() ) {
        return;
    }
    terminalActivity = std::chrono::steady_clock::now();
    if ( terminal.exited ) {
        startTerminal( terminal, terminalStartDirectory( filePath ), terminal.rows,
                       terminal.columns );
        return;
    }
    for ( const TerminalInput &input : inputs ) {
        ::sendTerminalInput( terminal, input );
    }
    if ( !paste.empty() ) {
        pasteToTerminal( terminal, paste );
    }
}

void Session::stopTerminal() {
    ::stopTerminal( terminal );
}

void Session::clearTerminal() {
    terminal.scrollback.clear();
    TerminalInput clearScreen;
    clearScreen.character = U'l';
    clearScreen.ctrl = true;
    sendTerminalInput( { clearScreen }, "" );
}

void Session::update() {
    updateRunningProgram();
    updateDebugger();
    if ( updateTerminal( terminal ) ) {
        terminalActivity = std::chrono::steady_clock::now();
    }
}

bool Session::isDebugging() const {
    return debugger.state != DebuggerState::Idle;
}

bool Session::isPaused() const {
    return debugger.state == DebuggerState::Paused;
}

bool Session::isProgramActive() const {
    return isRunning( runningProgram ) || isDebugging();
}

bool Session::isProgramBusy() const {
    return isRunning( runningProgram ) || debugger.state == DebuggerState::Running;
}

bool Session::readMemory( uint64_t address, uint8_t &byte ) {
    if ( !isPaused() ) {
        return false;
    }
    uint64_t blockAddress = address / memoryBlockSize * memoryBlockSize;
    auto block = memoryBlocks.find( blockAddress );
    if ( block == memoryBlocks.end() ) {
        std::vector<uint8_t> bytes;
        ::readMemory( debugger, blockAddress, memoryBlockSize, bytes );
        block = memoryBlocks.emplace( blockAddress, std::move( bytes ) ).first;
    }
    if ( block->second.empty() ) {
        return false;
    }
    byte = block->second[address - blockAddress];
    return true;
}

// Output often follows input by a few milliseconds, so the frame loop stays fast for a while.
bool Session::isTerminalBusy() const {
    return std::chrono::steady_clock::now() - terminalActivity < std::chrono::milliseconds( 500 );
}

bool Session::hasBreakpoint( const std::string &path, int line ) const {
    auto lines = breakpoints.find( path );
    return lines != breakpoints.end() && lines->second.contains( line );
}

// The path is copied first: it may be an element of recentFiles, which erase moves.
void Session::addRecentFile( const std::string &path ) {
    std::string recentPath = absolutePath( path );
    std::erase( recentFiles, recentPath );
    recentFiles.insert( recentFiles.begin(), recentPath );
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
        if ( !programLineOpen ) {
            console.push_back( {} );
            programLineOpen = true;
        }
        if ( c == '\n' ) {
            programLineOpen = false;
        } else {
            console.back().text += c;
        }
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

    console.clear();
    console.push_back( { "$ debug " + executablePath, MessageKind::Command } );
    if ( !startDebugger( debugger, executablePath, programFilePath ) ) {
        printResult( debugger.error, MessageKind::Error );
        return;
    }
    setBreakpointLines( debugger, breakpoints[programFilePath] );
    programLineOpen = false;
    registers = readRegisters( debugger );
    hasRegisters = true;

    if ( pauseAtEntry && sourceLineAt( debugger, registers.pc ) == 0 ) {
        // A C program starts in glibc's _start; the step goes to main.
        stepDebugger( debugger );
        handleDebuggerState();
    } else if ( pauseAtEntry ) {
        showPausedLine();
    } else {
        runDebugger( debugger );
        status = "Running in debugger";
    }
}

void Session::endDebugSession() {
    stopDebugger( debugger );
    pausedLine = 0;
    setLineMarks( {} );
}

void Session::handleDebuggerState() {
    appendProgramOutput( pollProcess( debugger.qemu ) );

    if ( debugger.state == DebuggerState::Paused ) {
        showPausedLine();
    } else if ( debugger.state == DebuggerState::Running ) {
        showRunning();
    } else if ( debugger.state == DebuggerState::Exited ) {
        int exitCode = debugger.exitCode;
        endDebugSession();
        printExitCode( exitCode );
    } else if ( debugger.state == DebuggerState::Crashed ) {
        showCrash();
    }
}

// A step can leave the program running, e.g. when it waits in read for input.
void Session::showRunning() {
    if ( pausedLine == 0 && !lineMarks.empty() ) {
        return;
    }
    pausedLine = 0;
    setLineMarks( {} );
    status = "Running in debugger";
}

void Session::showPausedLine() {
    previousRegisters = registers;
    memoryBlocks.clear();
    registers = readRegisters( debugger );
    pausedLine = sourceLineAt( debugger, registers.pc );
    if ( pausedLine > 0 ) {
        setLineMarks( { { pausedLine, LineMarkKind::Paused, "" } } );
    } else {
        setLineMarks( {} );
    }
    lineToReveal = pausedLine;
    status = "Paused at line " + std::to_string( pausedLine );
}

void Session::showCrash() {
    std::string error = debugger.error;
    int line = sourceLineAt( debugger, readRegisters( debugger ).pc );
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

void Session::updateDebugger() {
    if ( debugger.state != DebuggerState::Running ) {
        return;
    }
    pollDebugger( debugger, debuggerPollMilliseconds );
    handleDebuggerState();
}
