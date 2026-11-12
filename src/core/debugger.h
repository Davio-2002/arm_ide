#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <core/process.h>

enum class DebuggerState { Idle, Paused, Running, Exited, Crashed };

// A program under `qemu-aarch64 -g`, driven through QEMU's GDB remote protocol stub.
struct Debugger {
    Process qemu;
    int socket = -1;
    std::string received;
    std::map<uint64_t, int> sourceLines;
    std::set<uint64_t> breakpoints;
    // Set while resuming from a breakpoint: the instruction under it is stepped over first.
    bool continueAfterStep = false;
    // QEMU's single step runs one instruction too many after an svc, so stepping an svc runs to
    // a temporary breakpoint after it, with a breakpoint under the PC lifted meanwhile.
    uint64_t temporaryBreakpoint = 0;
    uint64_t liftedBreakpoint = 0;
    DebuggerState state = DebuggerState::Idle;
    int exitCode = 0;
    std::string error;
};

struct Registers {
    uint64_t x[31] = {};
    uint64_t sp = 0;
    uint64_t pc = 0;
    uint64_t nzcv = 0;
};

// Starts the program paused at its entry point.
bool startDebugger( Debugger &debugger, const std::string &executablePath );
void setBreakpointLines( Debugger &debugger, const std::set<int> &lines );
// Runs from the entry point: a breakpoint on the first instruction stops at once.
void runDebugger( Debugger &debugger );
// Resumes from a pause, stepping over a breakpoint under the program counter first.
void continueDebugger( Debugger &debugger );
void stepDebugger( Debugger &debugger );
// Waits up to timeoutMilliseconds for a running program to stop.
void pollDebugger( Debugger &debugger, int timeoutMilliseconds );
Registers readRegisters( Debugger &debugger );
bool readMemory( Debugger &debugger, uint64_t address, size_t size, std::vector<uint8_t> &bytes );
int sourceLineAt( const Debugger &debugger, uint64_t address );
void stopDebugger( Debugger &debugger );
