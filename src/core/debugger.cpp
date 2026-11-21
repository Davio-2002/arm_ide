#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <format>
#include <fstream>
#include <optional>
#include <thread>
#include <unistd.h>
#include <utility>

#include <core/debugger.h>
#include <core/toolchain.h>

namespace {

const int requestTimeoutMilliseconds = 2000;
const int connectTimeoutMilliseconds = 3000;
const int stepTimeoutMilliseconds = 200;
const int gdbSignalTrap = 5;
const size_t registerCount = 33;
/// Hex digits of one 64-bit register in a `g` reply.
const size_t registerDigits = 16;
const uint64_t flagsMask = 0xf0000000;
const auto interruptedResult = static_cast<uint64_t>( -EINTR );
/// AArch64 Linux system calls whose first argument QEMU passes to the host unchanged (an fd, a
/// pid, an id type or a clock): epoll_pwait, read, write, readv, writev, pread64, pwrite64,
/// pselect6, waitid, clock_nanosleep, accept, sendto, recvfrom, sendmsg, recvmsg, accept4, wait4.
constexpr auto restartableSystemCalls = std::to_array<uint64_t>(
    { 22, 63, 64, 65, 66, 67, 68, 72, 95, 115, 202, 206, 207, 211, 212, 242, 260 } );

/// @brief Names a GDB signal; GDB's numbers differ from Linux's from 7 on.
std::string signalName( int signal ) {
    static const std::array names = { "",
                                      "Hangup",
                                      "Interrupt",
                                      "Quit",
                                      "Illegal instruction",
                                      "Trace trap",
                                      "Aborted",
                                      "Emulation trap",
                                      "Floating point exception",
                                      "Killed",
                                      "Bus error",
                                      "Segmentation fault",
                                      "Bad system call",
                                      "Broken pipe",
                                      "Alarm clock",
                                      "Terminated" };
    if ( signal > 0 && std::cmp_less( signal, names.size() ) ) {
        return names[signal];
    }
    return "Signal " + std::to_string( signal );
}

/// @brief Sends a command and waits for its reply; marks the debugger crashed on no answer.
bool request( Debugger &debugger, const std::string &payload, std::string &reply ) {
    if ( !gdbRequest( debugger.gdb, payload, reply, requestTimeoutMilliseconds ) ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "QEMU did not answer";
        return false;
    }
    return true;
}

/// @brief Waits for a stop reply; a lost connection marks the debugger crashed.
bool receiveStopReply( Debugger &debugger, std::string &reply, int timeoutMilliseconds ) {
    GdbReceive const received = receiveGdbPacket( debugger.gdb, reply, timeoutMilliseconds );
    if ( received == GdbReceive::Closed ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "Lost connection to QEMU";
    }
    return received == GdbReceive::Packet;
}

/// @param[in] number GDB register number: 0-30 x0-x30, 31 sp, 32 pc.
/// @return Register value, or 0 on no answer.
uint64_t readRegister( Debugger &debugger, int number ) {
    std::string reply;
    if ( !request( debugger, "p" + toHex( number ), reply ) ) {
        return 0;
    }
    return littleEndian( reply, 0, 8 );
}

uint64_t programCounter( Debugger &debugger ) {
    return readRegister( debugger, 32 );
}

/// @brief Resumes without waiting for a stop.
/// @param[in] command "c" to continue or "s" to step.
void resume( Debugger &debugger, const char *command ) {
    debugger.state = DebuggerState::Running;
    if ( !sendGdbPacket( debugger.gdb, command ) ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "Lost connection to QEMU";
    }
}

/// @brief Continues after stepping off a breakpoint, unless the step hit another one.
void finishStepOverBreakpoint( Debugger &debugger ) {
    debugger.pendingStop = PendingStop::None;
    debugger.state = DebuggerState::Paused;
    if ( !debugger.breakpoints.contains( programCounter( debugger ) ) ) {
        resume( debugger, "c" );
    }
}

/// @return True if the instruction at the address is an svc.
bool isSystemCall( Debugger &debugger, uint64_t address ) {
    static const uint32_t svcMask = 0xffe0001f;
    static const uint32_t svcBits = 0xd4000001;
    std::vector<uint8_t> code;
    if ( !readMemory( debugger, address, 4, code ) ) {
        return false;
    }
    uint32_t const word =
        code[0] | code[1] << 8 | code[2] << 16 | static_cast<uint32_t>( code[3] ) << 24;
    return ( word & svcMask ) == svcBits;
}

/// @brief Continues to a temporary breakpoint, lifting one under the PC meanwhile, as QEMU
///        would stop on it at once.
void runTo( Debugger &debugger, uint64_t address ) {
    uint64_t const pc = programCounter( debugger );
    std::string reply;
    if ( debugger.breakpoints.contains( pc ) ) {
        request( debugger, "z0," + toHex( pc ) + ",4", reply );
        debugger.liftedBreakpoint = pc;
    }
    if ( !debugger.breakpoints.contains( address ) ) {
        request( debugger, "Z0," + toHex( address ) + ",4", reply );
        debugger.temporaryBreakpoint = address;
    }
    resume( debugger, "c" );
}

void writeRegister( Debugger &debugger, int number, uint64_t value ) {
    std::string reply;
    request( debugger, "P" + toHex( number ) + "=" + littleEndianHex( value ), reply );
}

/// @brief Waits until a SIGSTOP has stopped the process.
/// @return False if it did not stop in time.
bool waitUntilStopped( pid_t pid ) {
    std::string const path = std::format( "/proc/{}/stat", pid );
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds( requestTimeoutMilliseconds );
    while ( std::chrono::steady_clock::now() < deadline ) {
        std::string stat;
        std::getline( std::ifstream( path ), stat );
        size_t const nameEnd = stat.rfind( ')' );
        if ( nameEnd != std::string::npos && stat.substr( nameEnd + 1, 3 ) == " T " ) {
            return true;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
    return false;
}

/// @return First argument of the host system call the stopped process is blocked in, or
///         nothing when it is not in one.
std::optional<uint64_t> blockedSystemCallArgument( pid_t pid ) {
    std::ifstream file( std::format( "/proc/{}/syscall", pid ) );
    int64_t number = -1;
    std::string argument;
    if ( !( file >> number >> argument ) || number < 0 ) {
        return std::nullopt;
    }
    return parseHex( argument );
}

/// @brief Stops the running program with a SIGTRAP, which QEMU reports as a stop.
/// @param[out] reply           Stop reply.
/// @param[out] blockedArgument First argument of the system call the program was blocked in.
/// @return True if a stop reply arrived.
bool interruptProgram( Debugger &debugger, std::string &reply,
                       std::optional<uint64_t> &blockedArgument ) {
    pid_t const pid = debugger.qemu.pid;
    if ( kill( pid, SIGSTOP ) != 0 || !waitUntilStopped( pid ) ) {
        kill( pid, SIGCONT );
        return false;
    }
    blockedArgument = blockedSystemCallArgument( pid );
    bool const sent = kill( pid, SIGTRAP ) == 0;
    kill( pid, SIGCONT );
    return sent && receiveStopReply( debugger, reply, requestTimeoutMilliseconds );
}

/// @brief Restarts the system call that interrupting the program ended. The kernel restarts a
///        call for a signal the debugger drops, but QEMU returns EINTR to the program instead.
/// @param[in] firstArgument The call's first argument, read before the interrupt.
void restartInterruptedSystemCall( Debugger &debugger, uint64_t firstArgument ) {
    uint64_t const call = programCounter( debugger ) - 4;
    if ( readRegister( debugger, 0 ) != interruptedResult || !isSystemCall( debugger, call ) ||
         !std::ranges::contains( restartableSystemCalls, readRegister( debugger, 8 ) ) ) {
        return;
    }
    writeRegister( debugger, 0, firstArgument );
    writeRegister( debugger, 32, call );
}

/// @brief Steps one instruction. An svc runs to PC+4 instead: QEMU's single step runs one
///        instruction too many after it.
void stepInstruction( Debugger &debugger ) {
    uint64_t const pc = programCounter( debugger );
    if ( isSystemCall( debugger, pc ) ) {
        runTo( debugger, pc + 4 );
    } else {
        resume( debugger, "s" );
    }
}

/// @brief Steps again, or ends the line step at a new line or a breakpoint.
void continueLineStep( Debugger &debugger ) {
    uint64_t const pc = programCounter( debugger );
    int const line = sourceLineAt( debugger, pc );
    if ( line == 0 ) {
        uint64_t const returnAddress = readRegister( debugger, 30 );
        if ( returnAddress != pc && sourceLineAt( debugger, returnAddress ) != 0 ) {
            runTo( debugger, returnAddress );
        } else {
            debugger.pendingStop = PendingStop::None;
            resume( debugger, "c" );
        }
        return;
    }
    bool const atLineStart = debugger.sourceLines.contains( pc );
    bool const newLine = line != debugger.stepLine || pc == debugger.stepStart;
    if ( ( atLineStart && newLine ) || debugger.breakpoints.contains( pc ) ) {
        debugger.pendingStop = PendingStop::None;
    } else {
        stepInstruction( debugger );
    }
}

/// @brief Removes the temporary breakpoint and restores the lifted one, unless the user
///        changed them meanwhile.
void restoreStepBreakpoints( Debugger &debugger ) {
    std::string reply;
    uint64_t const temporary = debugger.temporaryBreakpoint;
    if ( temporary != 0 && !debugger.breakpoints.contains( temporary ) ) {
        request( debugger, "z0," + toHex( temporary ) + ",4", reply );
    }
    uint64_t const lifted = debugger.liftedBreakpoint;
    if ( lifted != 0 && debugger.breakpoints.contains( lifted ) ) {
        request( debugger, "Z0," + toHex( lifted ) + ",4", reply );
    }
    debugger.temporaryBreakpoint = 0;
    debugger.liftedBreakpoint = 0;
}

/// @brief Handles exit, kill, signal or trap; a trap continues a pending step.
/// @param[in] reply Stop reply, e.g. "W00" or "T05...".
void handleStopReply( Debugger &debugger, const std::string &reply ) {
    char const kind = reply.empty() ? '\0' : reply[0];
    if ( kind == 'W' ) {
        debugger.state = DebuggerState::Exited;
        debugger.exitCode = static_cast<int>( parseHex( reply.substr( 1 ) ) );
    } else if ( kind == 'X' ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error =
            "Killed: " + signalName( static_cast<int>( parseHex( reply.substr( 1, 2 ) ) ) );
    } else if ( kind == 'T' || kind == 'S' ) {
        int const signal = static_cast<int>( parseHex( reply.substr( 1, 2 ) ) );
        if ( signal != gdbSignalTrap ) {
            debugger.state = DebuggerState::Crashed;
            debugger.error = signalName( signal );
            return;
        }
        debugger.state = DebuggerState::Paused;
        restoreStepBreakpoints( debugger );
        if ( debugger.pendingStop == PendingStop::StepOverBreakpoint ) {
            finishStepOverBreakpoint( debugger );
        } else if ( debugger.pendingStop == PendingStop::LineStep ) {
            continueLineStep( debugger );
        }
    } else {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "Unexpected reply from QEMU: " + reply;
    }
}

/// @return Socket path unique to this IDE process.
std::string socketPath() {
    const char *runtime = std::getenv( "XDG_RUNTIME_DIR" );
    std::string const directory = runtime ? runtime : "/tmp";
    return directory + "/arm_ide-" + std::to_string( getpid() ) + ".sock";
}

/// @brief Connects to QEMU's socket, retrying until QEMU has created it.
/// @return True if connected; otherwise `error` says why.
bool connectToQemu( Debugger &debugger, const std::string &path ) {
    if ( !isValidSocketPath( path ) ) {
        debugger.error = "Socket path is too long: " + path;
        return false;
    }
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds( connectTimeoutMilliseconds );
    while ( std::chrono::steady_clock::now() < deadline ) {
        std::string const output = pollProcess( debugger.qemu );
        if ( !isRunning( debugger.qemu ) ) {
            debugger.error = "QEMU stopped: " + output;
            return false;
        }
        if ( connectGdb( debugger.gdb, path ) ) {
            return true;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
    debugger.error = "Cannot connect to QEMU";
    return false;
}

/// @brief Inserts and removes breakpoints in QEMU to match the addresses.
void applyBreakpoints( Debugger &debugger, const std::set<uint64_t> &addresses ) {
    std::string reply;
    for ( uint64_t const address : debugger.breakpoints ) {
        if ( !addresses.contains( address ) ) {
            request( debugger, "z0," + toHex( address ) + ",4", reply );
        }
    }
    for ( uint64_t const address : addresses ) {
        if ( !debugger.breakpoints.contains( address ) ) {
            request( debugger, "Z0," + toHex( address ) + ",4", reply );
        }
    }
    debugger.breakpoints = addresses;
}

} // anonymous namespace

bool startDebugger( Debugger &debugger, const std::string &executablePath,
                    const std::string &sourcePath ) {
    stopDebugger( debugger );
    debugger.sourceLines = readSourceLines( executablePath, sourcePath );
    debugger.mainAddress = symbolAddress( executablePath, "main" );

    std::string const path = socketPath();
    unlink( path.c_str() );
    std::string const qemu = qemuProgram();
    if ( qemu.empty() ) {
        debugger.error = qemuNotFound;
        return false;
    }
    debugger.qemu = startProcess( { qemu, "-g", path, executablePath } );
    if ( !isRunning( debugger.qemu ) ) {
        debugger.error = "Cannot start " + qemu;
        return false;
    }
    bool const connected = connectToQemu( debugger, path );
    unlink( path.c_str() );

    std::string reply;
    if ( !connected || !request( debugger, "?", reply ) || reply.empty() || reply[0] != 'T' ) {
        std::string const error =
            connected && debugger.error.empty() ? "QEMU did not stop at entry" : debugger.error;
        stopDebugger( debugger );
        debugger.error = error;
        return false;
    }
    debugger.state = DebuggerState::Paused;
    return true;
}

void setBreakpointLines( Debugger &debugger, const std::set<int> &lines ) {
    std::set<uint64_t> addresses;
    std::set<int> placed;
    for ( const auto &[address, line] : debugger.sourceLines ) {
        if ( lines.contains( line ) && placed.insert( line ).second ) {
            addresses.insert( address );
        }
    }

    if ( debugger.state == DebuggerState::Paused ) {
        applyBreakpoints( debugger, addresses );
    } else if ( debugger.state == DebuggerState::Running ) {
        std::string reply;
        std::optional<uint64_t> blockedArgument;
        if ( !interruptProgram( debugger, reply, blockedArgument ) ) {
            debugger.state = DebuggerState::Crashed;
            debugger.error = "QEMU did not stop";
            return;
        }
        if ( !reply.starts_with( "T05" ) && !reply.starts_with( "S05" ) ) {
            handleStopReply( debugger, reply );
            return;
        }
        debugger.state = DebuggerState::Paused;
        if ( blockedArgument ) {
            restartInterruptedSystemCall( debugger, *blockedArgument );
        }
        applyBreakpoints( debugger, addresses );
        if ( addresses.contains( programCounter( debugger ) ) ) {
            restoreStepBreakpoints( debugger );
            debugger.pendingStop = PendingStop::None;
        } else {
            resume( debugger, "c" );
        }
    }
}

void runDebugger( Debugger &debugger ) {
    if ( debugger.state == DebuggerState::Paused ) {
        resume( debugger, "c" );
    }
}

void continueDebugger( Debugger &debugger ) {
    if ( debugger.state != DebuggerState::Paused ) {
        return;
    }
    bool const onBreakpoint = debugger.breakpoints.contains( programCounter( debugger ) );
    debugger.pendingStop = onBreakpoint ? PendingStop::StepOverBreakpoint : PendingStop::None;
    if ( onBreakpoint ) {
        stepInstruction( debugger );
    } else {
        resume( debugger, "c" );
    }
}

void stepDebugger( Debugger &debugger ) {
    if ( debugger.state != DebuggerState::Paused ) {
        return;
    }
    uint64_t const pc = programCounter( debugger );
    int const line = sourceLineAt( debugger, pc );
    if ( line == 0 && debugger.mainAddress != 0 && pc != debugger.mainAddress ) {
        debugger.pendingStop = PendingStop::None;
        runTo( debugger, debugger.mainAddress );
    } else {
        debugger.pendingStop = PendingStop::LineStep;
        debugger.stepStart = pc;
        debugger.stepLine = line;
        stepInstruction( debugger );
    }
    pollDebugger( debugger, stepTimeoutMilliseconds );
}

void pollDebugger( Debugger &debugger, int timeoutMilliseconds ) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds( timeoutMilliseconds );
    std::string reply;
    while ( debugger.state == DebuggerState::Running ) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now() );
        if ( !receiveStopReply( debugger, reply,
                                static_cast<int>( std::max<int64_t>( 0, left.count() ) ) ) ) {
            return;
        }
        handleStopReply( debugger, reply );
    }
}

Registers readRegisters( Debugger &debugger ) {
    Registers registers;
    std::string reply;
    size_t const size = ( registerCount * registerDigits ) + 8;
    if ( !request( debugger, "g", reply ) || reply.size() < size ) {
        return registers;
    }
    for ( size_t i = 0; i < registers.x.size(); i++ ) {
        registers.x[i] = littleEndian( reply, i * registerDigits, 8 );
    }
    registers.sp = littleEndian( reply, 31 * registerDigits, 8 );
    registers.pc = littleEndian( reply, 32 * registerDigits, 8 );
    registers.nzcv = littleEndian( reply, registerCount * registerDigits, 4 ) & flagsMask;
    return registers;
}

bool readMemory( Debugger &debugger, uint64_t address, size_t size, std::vector<uint8_t> &bytes ) {
    std::string reply;
    std::string const command = "m" + toHex( address ) + "," + toHex( size );
    if ( !request( debugger, command, reply ) || reply.size() != size * 2 ) {
        return false;
    }
    bytes.resize( size );
    for ( size_t i = 0; i < size; i++ ) {
        bytes[i] = static_cast<uint8_t>( parseHex( reply.substr( i * 2, 2 ) ) );
    }
    return true;
}

int sourceLineAt( const Debugger &debugger, uint64_t address ) {
    auto next = debugger.sourceLines.upper_bound( address );
    if ( next == debugger.sourceLines.begin() ) {
        return 0;
    }
    return std::prev( next )->second;
}

bool parseAddress( const std::string &text, const Registers &registers, uint64_t &address ) {
    if ( text == "sp" ) {
        address = registers.sp;
        return true;
    }
    if ( text == "pc" ) {
        address = registers.pc;
        return true;
    }

    const char *start = text.c_str();
    bool const isRegister = text.starts_with( 'x' );
    if ( isRegister ) {
        start++;
    }
    char *end = nullptr; // NOLINT(misc-const-correctness): strtoull writes through &end
    uint64_t const value = std::strtoull( start, &end, isRegister ? 10 : 0 );
    if ( end == start || *end != '\0' ) {
        return false;
    }

    if ( !isRegister ) {
        address = value;
        return true;
    }
    if ( value > 30 ) {
        return false;
    }
    address = registers.x[value];
    return true;
}

void stopDebugger( Debugger &debugger ) {
    if ( isRunning( debugger.qemu ) ) {
        stopProcess( debugger.qemu );
    }
    closeGdb( debugger.gdb );
    debugger = Debugger();
}
