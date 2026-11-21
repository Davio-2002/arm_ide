#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <core/gdb_remote.h>
#include <core/process.h>

enum class DebuggerState { Idle, Paused, Running, Exited, Crashed };

enum class PendingStop {
    None,
    StepOverBreakpoint,
    LineStep,
};

/// @brief A program under `qemu-aarch64 -g`, driven through QEMU's GDB stub.
struct Debugger {
    Process qemu;
    GdbConnection gdb;
    DebuggerState state = DebuggerState::Idle;
    int exitCode = 0;
    std::string error;

    std::map<uint64_t, int> sourceLines;
    uint64_t mainAddress = 0;
    std::set<uint64_t> breakpoints;

    PendingStop pendingStop = PendingStop::None;
    uint64_t stepStart = 0;
    int stepLine = 0;
    uint64_t temporaryBreakpoint = 0;
    uint64_t liftedBreakpoint = 0;
};

struct Registers {
    std::array<uint64_t, 31> x = {};
    uint64_t sp = 0;
    uint64_t pc = 0;
    uint64_t nzcv = 0;
};

/// @brief Starts the program in QEMU, paused at its entry point.
/// @param[in,out] debugger       Debugger; a previous session is stopped.
/// @param[in]     executablePath Program to debug.
/// @param[in]     sourcePath     Source the line table maps to.
/// @return True if paused at entry; otherwise `error` says why.
bool startDebugger( Debugger &debugger, const std::string &executablePath,
                    const std::string &sourcePath );

/// @brief Sets breakpoints, stopping a running program briefly to apply them. A line's
///        breakpoint is at its first address, so Continue doesn't stop again on the same line.
/// @param[in,out] debugger Active debugger.
/// @param[in]     lines    1-based source lines; lines without code are ignored.
void setBreakpointLines( Debugger &debugger, const std::set<int> &lines );

/// @brief Runs from the entry point; a breakpoint on the first instruction still hits.
/// @param[in,out] debugger Debugger paused at entry.
void runDebugger( Debugger &debugger );

/// @brief Resumes, stepping off a breakpoint under the PC first.
/// @param[in,out] debugger Paused debugger.
void continueDebugger( Debugger &debugger );

/// @brief Runs to the next source line; calls into glibc are stepped over.
/// @param[in,out] debugger Paused debugger.
void stepDebugger( Debugger &debugger );

/// @brief Handles every stop reply that arrives within the timeout.
/// @param[in,out] debugger            Debugger to poll.
/// @param[in]     timeoutMilliseconds Longest wait.
void pollDebugger( Debugger &debugger, int timeoutMilliseconds );

/// @param[in,out] debugger Paused debugger.
/// @return Registers, or zeros if QEMU did not answer.
Registers readRegisters( Debugger &debugger );

/// @param[in,out] debugger Paused debugger.
/// @param[in]     address  First address.
/// @param[in]     size     Byte count.
/// @param[out]    bytes    Bytes read; unchanged on failure.
/// @return True if all bytes were read.
bool readMemory( Debugger &debugger, uint64_t address, size_t size, std::vector<uint8_t> &bytes );

/// @param[in] debugger Debugger with the line table.
/// @param[in] address  Code address.
/// @return 1-based line, or 0 outside the source.
int sourceLineAt( const Debugger &debugger, uint64_t address );

/// @brief Parses an address typed by the user.
/// @param[in]  text      "sp", "pc", x0..x30 or a C number.
/// @param[in]  registers Current register values.
/// @param[out] address   Parsed address.
/// @return True if valid.
bool parseAddress( const std::string &text, const Registers &registers, uint64_t &address );

/// @brief Kills QEMU and resets to idle.
/// @param[in,out] debugger Debugger to stop.
void stopDebugger( Debugger &debugger );
