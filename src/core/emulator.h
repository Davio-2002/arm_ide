#pragma once

#include <unicorn/unicorn.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

enum class EmulatorState { Idle, Paused, Running, Exited, Crashed };

struct Emulator {
    uc_engine *engine = nullptr;
    std::map<uint64_t, int> sourceLines;
    std::set<uint64_t> breakpoints;
    uint64_t skipBreakpointAt = UINT64_MAX;
    EmulatorState state = EmulatorState::Idle;
    std::string output;
    int exitCode = 0;
    std::string error;
};

struct Registers {
    uint64_t x[31] = {};
    uint64_t sp = 0;
    uint64_t pc = 0;
    uint64_t nzcv = 0;
};

bool loadProgram( Emulator &emulator, const std::string &executablePath );
void setBreakpointLines( Emulator &emulator, const std::set<int> &lines );
void continueEmulator( Emulator &emulator );
void runEmulator( Emulator &emulator, uint64_t timeoutMicroseconds );
void stepEmulator( Emulator &emulator );
uint64_t programCounter( const Emulator &emulator );
Registers readRegisters( const Emulator &emulator );
bool readMemoryByte( const Emulator &emulator, uint64_t address, uint8_t &byte );
int sourceLineAt( const Emulator &emulator, uint64_t address );
void closeEmulator( Emulator &emulator );
