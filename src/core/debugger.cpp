#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include <core/debugger.h>
#include <core/toolchain.h>

static const int requestTimeoutMilliseconds = 2000;
static const int connectTimeoutMilliseconds = 3000;
static const int stepTimeoutMilliseconds = 200;
static const int gdbSignalTrap = 5;
static const int registerCount = 33;
static const uint64_t flagsMask = 0xf0000000;

/// @brief Names a signal reported by the GDB stub.
///
/// The stub uses GDB's own signal numbers, which differ from Linux's from 7 on.
/// @param[in] signal GDB signal number.
/// @return Human-readable signal name.
static std::string signalName( int signal ) {
    static const char *names[] = { "",
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
    if ( signal > 0 && signal < static_cast<int>( std::size( names ) ) ) {
        return names[signal];
    }
    return "Signal " + std::to_string( signal );
}

/// @brief Formats a number as lower-case hex without a prefix, as the GDB protocol expects.
/// @param[in] value Number to format.
/// @return Hex digits.
static std::string toHex( uint64_t value ) {
    char text[17];
    std::snprintf( text, sizeof( text ), "%llx", static_cast<unsigned long long>( value ) );
    return text;
}

/// @brief Parses hex digits without a prefix.
/// @param[in] text Hex digits.
/// @return Parsed number; 0 for invalid text.
static uint64_t parseHex( const std::string &text ) {
    return std::strtoull( text.c_str(), nullptr, 16 );
}

/// @brief Decodes a little-endian number from a hex string, two digits per byte.
/// @param[in] hex       Hex string, as in register and memory replies.
/// @param[in] offset    Index of the first hex digit.
/// @param[in] byteCount Number of bytes to decode.
/// @return Decoded number.
static uint64_t littleEndian( const std::string &hex, size_t offset, size_t byteCount ) {
    uint64_t value = 0;
    for ( size_t i = 0; i < byteCount; i++ ) {
        value |= parseHex( hex.substr( offset + i * 2, 2 ) ) << ( i * 8 );
    }
    return value;
}

/// @brief Sends bytes to the GDB stub as they are.
/// @param[in] debugger Connected debugger.
/// @param[in] data     Bytes to send.
/// @return True if all bytes were sent.
static bool sendRaw( Debugger &debugger, const std::string &data ) {
    size_t sent = 0;
    while ( sent < data.size() ) {
        ssize_t count =
            send( debugger.socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL );
        if ( count <= 0 ) {
            return false;
        }
        sent += count;
    }
    return true;
}

/// @brief Sends a GDB packet: "$payload#xx" with its checksum.
/// @param[in] debugger Connected debugger.
/// @param[in] payload  Command, for example "g" or "m1000,4".
/// @return True if the packet was sent.
static bool sendPacket( Debugger &debugger, const std::string &payload ) {
    unsigned checksum = 0;
    for ( char c : payload ) {
        checksum += static_cast<unsigned char>( c );
    }
    char end[4];
    std::snprintf( end, sizeof( end ), "#%02x", checksum & 0xff );
    return sendRaw( debugger, "$" + payload + end );
}

/// @brief Takes one complete "$payload#xx" from the received bytes and acknowledges it.
///        Acknowledgements before it are skipped.
/// @param[in,out] debugger Debugger whose received bytes are consumed.
/// @param[out]    payload  Packet payload, without "$" and the checksum.
/// @return True if a complete packet was taken.
static bool takePacket( Debugger &debugger, std::string &payload ) {
    size_t start = debugger.received.find( '$' );
    if ( start == std::string::npos ) {
        debugger.received.clear();
        return false;
    }
    size_t end = debugger.received.find( '#', start );
    if ( end == std::string::npos || end + 2 >= debugger.received.size() ) {
        debugger.received.erase( 0, start );
        return false;
    }
    payload = debugger.received.substr( start + 1, end - start - 1 );
    debugger.received.erase( 0, end + 3 );
    sendRaw( debugger, "+" );
    return true;
}

/// @brief Waits for the next packet from the GDB stub.
/// @param[in,out] debugger            Connected debugger; a lost connection marks it crashed.
/// @param[out]    payload             Packet payload.
/// @param[in]     timeoutMilliseconds Longest time to wait.
/// @return True if a packet arrived in time.
static bool receivePacket( Debugger &debugger, std::string &payload, int timeoutMilliseconds ) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds( timeoutMilliseconds );
    while ( !takePacket( debugger, payload ) ) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now() );
        pollfd descriptor = { debugger.socket, POLLIN, 0 };
        if ( poll( &descriptor, 1, std::max<int>( 0, left.count() ) ) <= 0 ) {
            return false;
        }
        char buffer[4096];
        ssize_t count = recv( debugger.socket, buffer, sizeof( buffer ), 0 );
        if ( count <= 0 ) {
            debugger.state = DebuggerState::Crashed;
            debugger.error = "Lost connection to QEMU";
            return false;
        }
        debugger.received.append( buffer, count );
    }
    return true;
}

/// @brief Sends a command and waits for its reply.
/// @param[in,out] debugger Connected debugger; marked crashed if QEMU does not answer.
/// @param[in]     payload  Command.
/// @param[out]    reply    Reply payload.
/// @return True if a reply arrived.
static bool request( Debugger &debugger, const std::string &payload, std::string &reply ) {
    if ( debugger.socket < 0 || !sendPacket( debugger, payload ) ||
         !receivePacket( debugger, reply, requestTimeoutMilliseconds ) ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "QEMU did not answer";
        return false;
    }
    return true;
}

/// @brief Reads one register of the paused program.
/// @param[in,out] debugger Paused debugger.
/// @param[in]     number   GDB register number: 0-30 for x0-x30, 31 for sp, 32 for pc.
/// @return Register value, or 0 if QEMU did not answer.
static uint64_t readRegister( Debugger &debugger, int number ) {
    std::string reply;
    if ( !request( debugger, "p" + toHex( number ), reply ) ) {
        return 0;
    }
    return littleEndian( reply, 0, 8 );
}

/// @param[in,out] debugger Paused debugger.
/// @return Program counter of the paused program.
static uint64_t programCounter( Debugger &debugger ) {
    return readRegister( debugger, 32 );
}

/// @brief Resumes the program without waiting for it to stop.
/// @param[in,out] debugger Paused debugger; becomes running, or crashed if sending fails.
/// @param[in]     command  "c" to continue or "s" to step one instruction.
static void resume( Debugger &debugger, const char *command ) {
    debugger.state = DebuggerState::Running;
    if ( !sendPacket( debugger, command ) ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "Lost connection to QEMU";
    }
}

/// @brief Continues after the instruction under a breakpoint was stepped over, unless the
///        step landed on another breakpoint.
/// @param[in,out] debugger Debugger that just stopped after the step.
static void finishStepOverBreakpoint( Debugger &debugger ) {
    debugger.continueAfterStep = false;
    debugger.state = DebuggerState::Paused;
    if ( !debugger.breakpoints.contains( programCounter( debugger ) ) ) {
        resume( debugger, "c" );
    }
}

/// @brief Tells if the instruction at an address is an svc.
/// @param[in,out] debugger Paused debugger.
/// @param[in]     address  Instruction address.
/// @return True for an svc; false for other instructions or unreadable memory.
static bool isSystemCall( Debugger &debugger, uint64_t address ) {
    static const uint32_t svcMask = 0xffe0001f;
    static const uint32_t svcBits = 0xd4000001;
    std::vector<uint8_t> code;
    if ( !readMemory( debugger, address, 4, code ) ) {
        return false;
    }
    uint32_t word = code[0] | code[1] << 8 | code[2] << 16 | static_cast<uint32_t>( code[3] ) << 24;
    return ( word & svcMask ) == svcBits;
}

/// @brief Continues to a temporary breakpoint.
///
/// A breakpoint under the PC is lifted meanwhile, as QEMU would stop on it at once.
/// @param[in,out] debugger Paused debugger.
/// @param[in]     address  Address to stop at.
static void runTo( Debugger &debugger, uint64_t address ) {
    uint64_t pc = programCounter( debugger );
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

/// @brief Steps one instruction. An svc is run to the next instruction instead, because
///        QEMU's single step runs one instruction too many after it.
/// @param[in,out] debugger Paused debugger.
static void stepInstruction( Debugger &debugger ) {
    uint64_t pc = programCounter( debugger );
    if ( isSystemCall( debugger, pc ) ) {
        runTo( debugger, pc + 4 );
    } else {
        resume( debugger, "s" );
    }
}

/// @brief Takes the next step of a line step, or ends it at the start of a new line or at a
///        breakpoint.
/// @param[in,out] debugger Debugger stopped during a line step.
static void continueLineStep( Debugger &debugger ) {
    uint64_t pc = programCounter( debugger );
    int line = sourceLineAt( debugger, pc );
    if ( line == 0 ) {
        uint64_t returnAddress = readRegister( debugger, 30 );
        if ( returnAddress != pc && sourceLineAt( debugger, returnAddress ) != 0 ) {
            runTo( debugger, returnAddress );
        } else {
            debugger.steppingLine = false;
            resume( debugger, "c" );
        }
        return;
    }
    bool atLineStart = debugger.sourceLines.contains( pc );
    bool newLine = line != debugger.stepLine || pc == debugger.stepStart;
    if ( ( atLineStart && newLine ) || debugger.breakpoints.contains( pc ) ) {
        debugger.steppingLine = false;
    } else {
        stepInstruction( debugger );
    }
}

/// @brief Removes the temporary breakpoint and puts back the lifted one.
///
/// Breakpoints the user changed meanwhile stay as the user set them.
/// @param[in,out] debugger Paused debugger.
static void restoreStepBreakpoints( Debugger &debugger ) {
    std::string reply;
    uint64_t temporary = debugger.temporaryBreakpoint;
    if ( temporary != 0 && !debugger.breakpoints.contains( temporary ) ) {
        request( debugger, "z0," + toHex( temporary ) + ",4", reply );
    }
    uint64_t lifted = debugger.liftedBreakpoint;
    if ( lifted != 0 && debugger.breakpoints.contains( lifted ) ) {
        request( debugger, "Z0," + toHex( lifted ) + ",4", reply );
    }
    debugger.temporaryBreakpoint = 0;
    debugger.liftedBreakpoint = 0;
}

/// @brief Updates the debugger state from a stop reply: exit, kill, signal or trap.
///        A trap continues a pending step over a breakpoint or a line step.
/// @param[in,out] debugger Debugger that received the reply.
/// @param[in]     reply    Stop reply payload, such as "W00" or "T05...".
static void handleStopReply( Debugger &debugger, const std::string &reply ) {
    char kind = reply.empty() ? '\0' : reply[0];
    if ( kind == 'W' ) {
        debugger.state = DebuggerState::Exited;
        debugger.exitCode = static_cast<int>( parseHex( reply.substr( 1 ) ) );
    } else if ( kind == 'X' ) {
        debugger.state = DebuggerState::Crashed;
        debugger.error =
            "Killed: " + signalName( static_cast<int>( parseHex( reply.substr( 1, 2 ) ) ) );
    } else if ( kind == 'T' || kind == 'S' ) {
        int signal = static_cast<int>( parseHex( reply.substr( 1, 2 ) ) );
        if ( signal != gdbSignalTrap ) {
            debugger.state = DebuggerState::Crashed;
            debugger.error = signalName( signal );
            return;
        }
        debugger.state = DebuggerState::Paused;
        restoreStepBreakpoints( debugger );
        if ( debugger.continueAfterStep ) {
            finishStepOverBreakpoint( debugger );
        } else if ( debugger.steppingLine ) {
            continueLineStep( debugger );
        }
    } else {
        debugger.state = DebuggerState::Crashed;
        debugger.error = "Unexpected reply from QEMU: " + reply;
    }
}

/// @return Path of the Unix socket QEMU listens on, unique to this IDE process.
static std::string socketPath() {
    const char *runtime = std::getenv( "XDG_RUNTIME_DIR" );
    std::string directory = runtime ? runtime : "/tmp";
    return directory + "/arm_ide-" + std::to_string( getpid() ) + ".sock";
}

/// @brief Connects to QEMU's GDB socket, retrying until QEMU has created it.
/// @param[in,out] debugger Debugger with QEMU started; gets the socket.
/// @param[in]     path     Socket path passed to QEMU.
/// @return True if connected; otherwise debugger.error holds the reason.
static bool connectToQemu( Debugger &debugger, const std::string &path ) {
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    if ( path.size() >= sizeof( address.sun_path ) ) {
        debugger.error = "Socket path is too long: " + path;
        return false;
    }
    std::copy( path.begin(), path.end(), address.sun_path );

    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds( connectTimeoutMilliseconds );
    while ( std::chrono::steady_clock::now() < deadline ) {
        std::string output = pollProcess( debugger.qemu );
        if ( !isRunning( debugger.qemu ) ) {
            debugger.error = "QEMU stopped: " + output;
            return false;
        }
        debugger.socket = socket( AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0 );
        if ( connect( debugger.socket, reinterpret_cast<sockaddr *>( &address ),
                      sizeof( address ) ) == 0 ) {
            return true;
        }
        close( debugger.socket );
        debugger.socket = -1;
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
    debugger.error = "Cannot connect to QEMU";
    return false;
}

bool startDebugger( Debugger &debugger, const std::string &executablePath,
                    const std::string &sourcePath ) {
    stopDebugger( debugger );
    debugger.sourceLines = readSourceLines( executablePath, sourcePath );
    debugger.mainAddress = symbolAddress( executablePath, "main" );

    std::string path = socketPath();
    unlink( path.c_str() );
    std::string qemu = qemuProgram();
    if ( qemu.empty() ) {
        debugger.error = qemuNotFound;
        return false;
    }
    debugger.qemu = startProcess( { qemu, "-g", path, executablePath } );
    if ( !isRunning( debugger.qemu ) ) {
        debugger.error = "Cannot start " + qemu;
        return false;
    }
    bool connected = connectToQemu( debugger, path );
    unlink( path.c_str() );

    std::string reply;
    if ( !connected || !request( debugger, "?", reply ) || reply.empty() || reply[0] != 'T' ) {
        std::string error =
            connected && debugger.error.empty() ? "QEMU did not stop at entry" : debugger.error;
        stopDebugger( debugger );
        debugger.error = error;
        return false;
    }
    debugger.state = DebuggerState::Paused;
    return true;
}

/// @brief Removes and inserts breakpoints in QEMU so they match the given addresses.
/// @param[in,out] debugger  Paused debugger.
/// @param[in]     addresses New breakpoint addresses.
static void applyBreakpoints( Debugger &debugger, const std::set<uint64_t> &addresses ) {
    std::string reply;
    for ( uint64_t address : debugger.breakpoints ) {
        if ( !addresses.contains( address ) ) {
            request( debugger, "z0," + toHex( address ) + ",4", reply );
        }
    }
    for ( uint64_t address : addresses ) {
        if ( !debugger.breakpoints.contains( address ) ) {
            request( debugger, "Z0," + toHex( address ) + ",4", reply );
        }
    }
    debugger.breakpoints = addresses;
}

void setBreakpointLines( Debugger &debugger, const std::set<int> &lines ) {
    std::set<uint64_t> addresses;
    for ( const auto &[address, line] : debugger.sourceLines ) {
        if ( lines.contains( line ) ) {
            addresses.insert( address );
        }
    }

    if ( debugger.state == DebuggerState::Paused ) {
        applyBreakpoints( debugger, addresses );
    } else if ( debugger.state == DebuggerState::Running ) {
        std::string reply;
        if ( kill( debugger.qemu.pid, SIGTRAP ) != 0 ||
             !receivePacket( debugger, reply, requestTimeoutMilliseconds ) ) {
            debugger.state = DebuggerState::Crashed;
            debugger.error = "QEMU did not stop";
            return;
        }
        if ( !reply.starts_with( "T05" ) && !reply.starts_with( "S05" ) ) {
            handleStopReply( debugger, reply );
            return;
        }
        debugger.state = DebuggerState::Paused;
        applyBreakpoints( debugger, addresses );
        if ( addresses.contains( programCounter( debugger ) ) ) {
            restoreStepBreakpoints( debugger );
            debugger.continueAfterStep = false;
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
    debugger.steppingLine = false;
    debugger.continueAfterStep = debugger.breakpoints.contains( programCounter( debugger ) );
    if ( debugger.continueAfterStep ) {
        stepInstruction( debugger );
    } else {
        resume( debugger, "c" );
    }
}

void stepDebugger( Debugger &debugger ) {
    if ( debugger.state != DebuggerState::Paused ) {
        return;
    }
    debugger.continueAfterStep = false;
    uint64_t pc = programCounter( debugger );
    int line = sourceLineAt( debugger, pc );
    if ( line == 0 && debugger.mainAddress != 0 && pc != debugger.mainAddress ) {
        runTo( debugger, debugger.mainAddress );
    } else {
        debugger.steppingLine = true;
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
        if ( !receivePacket( debugger, reply, std::max<int>( 0, left.count() ) ) ) {
            return;
        }
        handleStopReply( debugger, reply );
    }
}

Registers readRegisters( Debugger &debugger ) {
    Registers registers;
    std::string reply;
    size_t size = registerCount * 16 + 8;
    if ( !request( debugger, "g", reply ) || reply.size() < size ) {
        return registers;
    }
    for ( int i = 0; i < 31; i++ ) {
        registers.x[i] = littleEndian( reply, i * 16, 8 );
    }
    registers.sp = littleEndian( reply, 31 * 16, 8 );
    registers.pc = littleEndian( reply, 32 * 16, 8 );
    registers.nzcv = littleEndian( reply, registerCount * 16, 4 ) & flagsMask;
    return registers;
}

bool readMemory( Debugger &debugger, uint64_t address, size_t size, std::vector<uint8_t> &bytes ) {
    std::string reply;
    std::string command = "m" + toHex( address ) + "," + toHex( size );
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

void stopDebugger( Debugger &debugger ) {
    if ( isRunning( debugger.qemu ) ) {
        stopProcess( debugger.qemu );
    }
    if ( debugger.socket >= 0 ) {
        close( debugger.socket );
    }
    debugger = Debugger();
}
