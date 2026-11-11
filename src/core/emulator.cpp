#include <cstring>
#include <elf.h>
#include <fstream>
#include <iterator>
#include <vector>

#include <core/emulator.h>
#include <core/toolchain.h>

static const uint64_t pageSize = 0x1000;
static const uint64_t stackTop = 0x80000000;
static const uint64_t stackSize = 0x100000;
static const uint64_t maxWriteSize = 0x100000;
static const uint64_t noAddress = UINT64_MAX;

static const uint32_t svcException = 2;
static const uint64_t syscallRead = 63;
static const uint64_t syscallWrite = 64;
static const uint64_t syscallExit = 93;
static const uint64_t syscallExitGroup = 94;
static const int64_t errorBadFile = -9;
static const int64_t errorBadAddress = -14;

static uint64_t readRegister( uc_engine *engine, int reg ) {
    uint64_t value = 0;
    uc_reg_read( engine, reg, &value );
    return value;
}

static void writeRegister( uc_engine *engine, int reg, uint64_t value ) {
    uc_reg_write( engine, reg, &value );
}

static void crash( Emulator &emulator, const std::string &message ) {
    emulator.state = EmulatorState::Crashed;
    emulator.error = message;
    uc_emu_stop( emulator.engine );
}

static int64_t writeToOutput( Emulator &emulator, uint64_t fd, uint64_t address, uint64_t size ) {
    if ( fd != 1 && fd != 2 ) {
        return errorBadFile;
    }

    std::string text( std::min( size, maxWriteSize ), '\0' );
    if ( uc_mem_read( emulator.engine, address, text.data(), text.size() ) != UC_ERR_OK ) {
        return errorBadAddress;
    }
    emulator.output += text;
    return text.size();
}

static void handleSystemCall( uc_engine *engine, uint32_t exception, void *userData ) {
    Emulator &emulator = *static_cast<Emulator *>( userData );
    if ( exception != svcException ) {
        crash( emulator, "Unexpected CPU exception " + std::to_string( exception ) );
        return;
    }

    uint64_t number = readRegister( engine, UC_ARM64_REG_X8 );
    uint64_t arg0 = readRegister( engine, UC_ARM64_REG_X0 );
    uint64_t arg1 = readRegister( engine, UC_ARM64_REG_X1 );
    uint64_t arg2 = readRegister( engine, UC_ARM64_REG_X2 );

    if ( number == syscallWrite ) {
        writeRegister( engine, UC_ARM64_REG_X0, writeToOutput( emulator, arg0, arg1, arg2 ) );
    } else if ( number == syscallRead ) {
        writeRegister( engine, UC_ARM64_REG_X0, 0 );
    } else if ( number == syscallExit || number == syscallExitGroup ) {
        emulator.state = EmulatorState::Exited;
        emulator.exitCode = static_cast<uint8_t>( arg0 );
        uc_emu_stop( engine );
    } else {
        crash( emulator, "Unsupported system call " + std::to_string( number ) );
    }
}

static void checkBreakpoint( uc_engine *engine, uint64_t address, uint32_t, void *userData ) {
    Emulator &emulator = *static_cast<Emulator *>( userData );
    if ( address == emulator.skipBreakpointAt ) {
        emulator.skipBreakpointAt = noAddress;
        return;
    }
    if ( emulator.breakpoints.contains( address ) ) {
        emulator.state = EmulatorState::Paused;
        uc_emu_stop( engine );
    }
}

static uint32_t segmentPermissions( uint32_t flags ) {
    uint32_t permissions = UC_PROT_NONE;
    if ( flags & PF_R ) {
        permissions |= UC_PROT_READ;
    }
    if ( flags & PF_W ) {
        permissions |= UC_PROT_WRITE;
    }
    if ( flags & PF_X ) {
        permissions |= UC_PROT_EXEC;
    }
    return permissions;
}

static bool loadSegments( Emulator &emulator, const std::vector<char> &file, uint64_t &entry ) {
    if ( file.size() < sizeof( Elf64_Ehdr ) ) {
        emulator.error = "File is too small to be an ELF executable";
        return false;
    }

    Elf64_Ehdr header;
    std::memcpy( &header, file.data(), sizeof( header ) );
    bool isElf = std::memcmp( header.e_ident, ELFMAG, SELFMAG ) == 0;
    if ( !isElf || header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_machine != EM_AARCH64 ) {
        emulator.error = "Not an AArch64 ELF executable";
        return false;
    }

    std::vector<Elf64_Phdr> segments;
    for ( int i = 0; i < header.e_phnum; i++ ) {
        uint64_t offset = header.e_phoff + i * sizeof( Elf64_Phdr );
        if ( offset + sizeof( Elf64_Phdr ) > file.size() ) {
            emulator.error = "Broken program header table";
            return false;
        }
        Elf64_Phdr segment;
        std::memcpy( &segment, file.data() + offset, sizeof( segment ) );
        bool fitsInFile = segment.p_offset + segment.p_filesz <= file.size();
        if ( segment.p_type == PT_LOAD && fitsInFile && segment.p_filesz <= segment.p_memsz ) {
            segments.push_back( segment );
        }
    }

    std::map<uint64_t, uint32_t> pages;
    for ( const Elf64_Phdr &segment : segments ) {
        uint64_t first = segment.p_vaddr / pageSize * pageSize;
        uint64_t end = segment.p_vaddr + segment.p_memsz;
        for ( uint64_t page = first; page < end; page += pageSize ) {
            pages[page] |= segmentPermissions( segment.p_flags );
        }
    }
    for ( const auto &[page, permissions] : pages ) {
        if ( uc_mem_map( emulator.engine, page, pageSize, permissions ) != UC_ERR_OK ) {
            emulator.error = "Cannot map program memory";
            return false;
        }
    }

    for ( const Elf64_Phdr &segment : segments ) {
        const char *bytes = file.data() + segment.p_offset;
        uc_mem_write( emulator.engine, segment.p_vaddr, bytes, segment.p_filesz );
    }

    entry = header.e_entry;
    return true;
}

bool loadProgram( Emulator &emulator, const std::string &executablePath ) {
    closeEmulator( emulator );
    emulator = Emulator();

    std::ifstream stream( executablePath, std::ios::binary );
    if ( !stream ) {
        emulator.error = "Cannot read " + executablePath;
        return false;
    }
    std::vector<char> file( ( std::istreambuf_iterator<char>( stream ) ),
                            std::istreambuf_iterator<char>() );

    if ( uc_open( UC_ARCH_ARM64, UC_MODE_ARM, &emulator.engine ) != UC_ERR_OK ) {
        emulator.error = "Cannot start Unicorn";
        return false;
    }

    uint64_t entry = 0;
    if ( !loadSegments( emulator, file, entry ) ) {
        closeEmulator( emulator );
        return false;
    }

    uc_mem_map( emulator.engine, stackTop - stackSize, stackSize, UC_PROT_READ | UC_PROT_WRITE );
    writeRegister( emulator.engine, UC_ARM64_REG_SP, stackTop - pageSize );
    writeRegister( emulator.engine, UC_ARM64_REG_PC, entry );

    uc_hook systemCallHook;
    uc_hook_add( emulator.engine, &systemCallHook, UC_HOOK_INTR, ( void * )handleSystemCall,
                 &emulator, 1, 0 );
    uc_hook breakpointHook;
    uc_hook_add( emulator.engine, &breakpointHook, UC_HOOK_CODE, ( void * )checkBreakpoint,
                 &emulator, 1, 0 );

    emulator.sourceLines = readSourceLines( executablePath );
    emulator.state = EmulatorState::Paused;
    return true;
}

void setBreakpointLines( Emulator &emulator, const std::set<int> &lines ) {
    emulator.breakpoints.clear();
    for ( const auto &[address, line] : emulator.sourceLines ) {
        if ( lines.contains( line ) ) {
            emulator.breakpoints.insert( address );
        }
    }
}

static void emulate( Emulator &emulator, uint64_t timeoutMicroseconds, size_t instructionCount ) {
    uint64_t start = programCounter( emulator );
    uc_err result =
        uc_emu_start( emulator.engine, start, UINT64_MAX, timeoutMicroseconds, instructionCount );
    if ( result != UC_ERR_OK && emulator.state == EmulatorState::Running ) {
        crash( emulator, uc_strerror( result ) );
    }
}

void continueEmulator( Emulator &emulator ) {
    emulator.skipBreakpointAt = programCounter( emulator );
    emulator.state = EmulatorState::Running;
}

void runEmulator( Emulator &emulator, uint64_t timeoutMicroseconds ) {
    emulate( emulator, timeoutMicroseconds, 0 );
}

void stepEmulator( Emulator &emulator ) {
    continueEmulator( emulator );
    emulate( emulator, 0, 1 );
    if ( emulator.state == EmulatorState::Running ) {
        emulator.state = EmulatorState::Paused;
    }
}

uint64_t programCounter( const Emulator &emulator ) {
    return readRegister( emulator.engine, UC_ARM64_REG_PC );
}

static int generalRegister( int index ) {
    if ( index == 29 ) {
        return UC_ARM64_REG_X29;
    }
    if ( index == 30 ) {
        return UC_ARM64_REG_X30;
    }
    return UC_ARM64_REG_X0 + index;
}

Registers readRegisters( const Emulator &emulator ) {
    Registers registers;
    for ( int i = 0; i < 31; i++ ) {
        registers.x[i] = readRegister( emulator.engine, generalRegister( i ) );
    }
    registers.sp = readRegister( emulator.engine, UC_ARM64_REG_SP );
    registers.pc = readRegister( emulator.engine, UC_ARM64_REG_PC );
    registers.nzcv = readRegister( emulator.engine, UC_ARM64_REG_NZCV );
    return registers;
}

bool readMemoryByte( const Emulator &emulator, uint64_t address, uint8_t &byte ) {
    return uc_mem_read( emulator.engine, address, &byte, 1 ) == UC_ERR_OK;
}

int sourceLineAt( const Emulator &emulator, uint64_t address ) {
    auto next = emulator.sourceLines.upper_bound( address );
    if ( next == emulator.sourceLines.begin() ) {
        return 0;
    }
    return std::prev( next )->second;
}

void closeEmulator( Emulator &emulator ) {
    if ( emulator.engine ) {
        uc_close( emulator.engine );
        emulator.engine = nullptr;
    }
}
