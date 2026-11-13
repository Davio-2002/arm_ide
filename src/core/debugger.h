#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <core/process.h>

/// @brief State of a debug session.
enum class DebuggerState { Idle, Paused, Running, Exited, Crashed };

/// @brief A program under `qemu-aarch64 -g`, driven through QEMU's GDB remote protocol stub.
struct Debugger {
    Process qemu;
    int socket = -1;
    std::string received;
    std::map<uint64_t, int> sourceLines;
    std::set<uint64_t> breakpoints;
    bool continueAfterStep = false;
    uint64_t temporaryBreakpoint = 0;
    uint64_t liftedBreakpoint = 0;
    bool steppingLine = false;
    uint64_t stepStart = 0;
    int stepLine = 0;
    uint64_t mainAddress = 0;
    DebuggerState state = DebuggerState::Idle;
    int exitCode = 0;
    std::string error;
};

/// @brief AArch64 general registers of the debugged program.
struct Registers {
    uint64_t x[31] = {};
    uint64_t sp = 0;
    uint64_t pc = 0;
    uint64_t nzcv = 0;
};

/// @brief Starts the program in QEMU, paused at its entry point.
/// @param[in,out] debugger       Debugger to start; any previous session is stopped.
/// @param[in]     executablePath Program to debug.
/// @param[in]     sourcePath     Source file whose lines the debugger maps addresses to.
/// @return True if the program is paused at entry; otherwise debugger.error holds the reason.
bool startDebugger( Debugger &debugger, const std::string &executablePath,
                    const std::string &sourcePath );

/// @brief Sets the breakpoints to the given source lines. A running program is stopped
///        briefly to apply them.
/// @param[in,out] debugger Active debugger.
/// @param[in]     lines    1-based source lines; lines without code are ignored.
void setBreakpointLines( Debugger &debugger, const std::set<int> &lines );

/// @brief Runs from the entry point. A breakpoint on the first instruction stops at once.
/// @param[in,out] debugger Debugger paused at entry.
void runDebugger( Debugger &debugger );

/// @brief Resumes from a pause, stepping over a breakpoint under the program counter first.
/// @param[in,out] debugger Paused debugger; does nothing otherwise.
void continueDebugger( Debugger &debugger );

/// @brief Runs to the next line of the source.
///
/// A call into code without lines (glibc) runs until it returns. From code without lines the
/// step goes to main.
/// @param[in,out] debugger Paused debugger; does nothing otherwise.
void stepDebugger( Debugger &debugger );

/// @brief Waits for a running program to stop and handles every stop reply that arrives.
/// @param[in,out] debugger            Debugger to poll.
/// @param[in]     timeoutMilliseconds Longest time to wait.
void pollDebugger( Debugger &debugger, int timeoutMilliseconds );

/// @brief Reads all general registers of the paused program.
/// @param[in,out] debugger Paused debugger.
/// @return The registers, or all zeros if QEMU did not answer.
Registers readRegisters( Debugger &debugger );

/// @brief Reads program memory.
/// @param[in,out] debugger Paused debugger.
/// @param[in]     address  First address to read.
/// @param[in]     size     Number of bytes.
/// @param[out]    bytes    Bytes read; unchanged on failure.
/// @return True if all bytes were read.
bool readMemory( Debugger &debugger, uint64_t address, size_t size, std::vector<uint8_t> &bytes );

/// @brief Finds the source line of an address.
/// @param[in] debugger Debugger with the line table loaded.
/// @param[in] address  Code address.
/// @return 1-based source line, or 0 for code outside the source.
int sourceLineAt( const Debugger &debugger, uint64_t address );

/// @brief Kills QEMU, closes the connection and resets the debugger to idle.
/// @param[in,out] debugger Debugger to stop.
void stopDebugger( Debugger &debugger );
