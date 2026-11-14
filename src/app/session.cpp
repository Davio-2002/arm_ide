#include <cstring>

#include <app/session.h>

namespace {

const int debuggerPollMilliseconds = 1;
const uint64_t memoryBlockSize = 256;

} // anonymous namespace

Session::Session() {
    recentFiles = readRecentFiles();
    addDocument( "", "" );
}

// Commands ----------------------------------------------------------------------------------

std::string Session::commandLabel( Command command ) const {
    switch ( command ) {
    case Command::NewFile:
        return "New File";
    case Command::Open:
        return "Open...";
    case Command::Save:
        return "Save";
    case Command::SaveAs:
        return "Save As...";
    case Command::Build:
        return "Build";
    case Command::Run:
        return "Build and Run";
    case Command::Stop:
        return "Stop";
    case Command::DebugContinue:
        return isDebugging() ? "Continue" : "Start Debugging";
    case Command::DebugStep:
        return "Step";
    case Command::ToggleBreakpoint:
        return "Toggle Breakpoint";
    case Command::ClearOutput:
        return "Clear Output";
    }
    return "";
}

bool Session::canRun( Command command ) const {
    switch ( command ) {
    case Command::Stop:
        return isProgramActive();
    case Command::DebugContinue:
    case Command::DebugStep:
        return !isDebugging() || isPaused();
    default:
        return true;
    }
}

PathRequest Session::pathNeeded( Command command ) const {
    bool untitled = activeDocument().path.empty();
    switch ( command ) {
    case Command::Open:
        return PathRequest::FileToOpen;
    case Command::SaveAs:
        return PathRequest::FileToSave;
    case Command::Save:
    case Command::Build:
    case Command::Run:
        return untitled ? PathRequest::FileToSave : PathRequest::None;
    case Command::DebugContinue:
    case Command::DebugStep:
        return untitled && !isDebugging() ? PathRequest::FileToSave : PathRequest::None;
    default:
        return PathRequest::None;
    }
}

void Session::run( Command command, const CommandInput &input ) {
    switch ( command ) {
    case Command::NewFile:
        activeDocumentId = addDocument( "", "" );
        break;
    case Command::Open:
        openFile( input.path );
        break;
    case Command::Save:
    case Command::SaveAs:
        saveActiveDocument( input.path.empty() ? activeDocument().path : input.path, input );
        break;
    case Command::Build:
        if ( saveBeforeBuild( input ) ) {
            build();
        }
        break;
    case Command::Run:
        if ( saveBeforeBuild( input ) ) {
            runProgram();
        }
        break;
    case Command::Stop:
        if ( isProgramActive() ) {
            stopActiveProgram();
            printResult( "Program stopped", MessageKind::Warning );
        }
        break;
    case Command::DebugContinue:
        if ( isDebugging() || saveBeforeBuild( input ) ) {
            debugContinue();
        }
        break;
    case Command::DebugStep:
        if ( isDebugging() || saveBeforeBuild( input ) ) {
            debugStep();
        }
        break;
    case Command::ToggleBreakpoint:
        toggleBreakpoint( activeDocument().path, input.line );
        break;
    case Command::ClearOutput:
        clearOutput();
        break;
    }
}

// Program -----------------------------------------------------------------------------------

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

bool Session::hasBreakpoint( const std::string &path, int line ) const {
    auto lines = breakpoints.find( path );
    return lines != breakpoints.end() && lines->second.contains( line );
}

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

bool Session::acceptsInput() const {
    return isRunning( runningProgram ) || isRunning( debugger.qemu );
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

void Session::update() {
    updateRunningProgram();
    updateDebugger();
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

void Session::stop() {
    stopActiveProgram();
}

/// Fills the build log and the line marks with the build messages.
bool Session::build() {
    stopActiveProgram();
    buildLog.clear();
    executablePath.clear();
    programFilePath = activeDocument().path;

    BuildResult build = buildProgram( programFilePath );
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

void Session::runProgram() {
    if ( !build() ) {
        return;
    }

    console.clear();
    std::string qemu = qemuProgram();
    if ( qemu.empty() ) {
        printResult( qemuNotFound, MessageKind::Error );
        return;
    }
    console.push_back( { "$ " + qemu + " " + executablePath, MessageKind::Command } );
    runningProgram = startProcess( { qemu, executablePath } );
    if ( !isRunning( runningProgram ) ) {
        printResult( "Cannot start " + qemu, MessageKind::Error );
        return;
    }
    programLineOpen = false;
    status = "Running " + executablePath;
}

/// Starts debugging, or resumes the program if it is paused.
void Session::debugContinue() {
    if ( !isDebugging() ) {
        startDebugging( false );
    } else if ( isPaused() ) {
        continueDebugger( debugger );
        handleDebuggerState();
    }
}

/// Starts debugging paused at the first line, or steps to the next line if paused.
void Session::debugStep() {
    if ( !isDebugging() ) {
        startDebugging( true );
    } else if ( isPaused() ) {
        stepDebugger( debugger );
        handleDebuggerState();
    }
}

void Session::clearOutput() {
    console.clear();
    buildLog.clear();
    programLineOpen = false;
}

void Session::setLineMarks( const std::vector<LineMark> &marks ) {
    lineMarks = marks;
    lineMarksVersion++;
}

/// Splits output into console lines; it may end in the middle of a line.
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

/// Prints a result line to the console and shows it as the status.
void Session::printResult( const std::string &text, MessageKind kind, int sourceLine ) {
    programLineOpen = false;
    console.push_back( { text, kind, sourceLine } );
    status = text;
}

void Session::printExitCode( int exitCode ) {
    std::string text = "Program exited with code " + std::to_string( exitCode );
    printResult( text, exitCode == 0 ? MessageKind::Success : MessageKind::Error );
}

/// @return The process reading program input, or nullptr if no program runs.
Process *Session::programProcess() {
    if ( isRunning( runningProgram ) ) {
        return &runningProgram;
    }
    if ( isRunning( debugger.qemu ) ) {
        return &debugger.qemu;
    }
    return nullptr;
}

void Session::stopActiveProgram() {
    if ( isRunning( runningProgram ) ) {
        stopProcess( runningProgram );
    }
    if ( isDebugging() ) {
        endDebugSession();
    }
}

/// Builds the active document and starts it under the debugger, paused at the first line or
/// running to a breakpoint.
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
    paused.registers = readRegisters( debugger );
    paused.hasRegisters = true;

    if ( pauseAtEntry && sourceLineAt( debugger, paused.registers.pc ) == 0 ) {
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
    paused.line = 0;
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

/// Clears the paused line once the program leaves a pause, for example while a step waits in
/// read for input.
void Session::showRunning() {
    if ( paused.line == 0 ) {
        return;
    }
    paused.line = 0;
    setLineMarks( {} );
    status = "Running in debugger";
}

/// Reads the registers and marks the line the debugger is paused on.
void Session::showPausedLine() {
    paused.previousRegisters = paused.registers;
    memoryBlocks.clear();
    paused.registers = readRegisters( debugger );
    paused.line = sourceLineAt( debugger, paused.registers.pc );
    if ( paused.line > 0 ) {
        setLineMarks( { { paused.line, LineMarkKind::Paused, "" } } );
        showProgramLine( paused.line );
    } else {
        setLineMarks( {} );
    }
    status = "Paused at line " + std::to_string( paused.line );
}

/// Ends the debug session and reports the crash with its line, if known.
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
