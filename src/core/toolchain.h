#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class MessageKind { Output, Command, Error, Warning, Success };

struct Message {
    std::string text;
    MessageKind kind = MessageKind::Output;
    int sourceLine = 0;
};

enum class SourceLanguage { Assembly, C };

/// @return C for .c and .h, AArch64 assembly otherwise.
SourceLanguage sourceLanguage( const std::string &path );

struct BuildResult {
    bool success = false;
    std::vector<Message> messages;
    std::string executablePath;
};

inline const std::string qemuNotFound =
    "qemu-aarch64 not found in PATH: install QEMU user-mode emulation (qemu-user)";

/// @brief Names a cross tool; the binutils prefix differs by distribution.
/// @param[in] name as, ld, gcc, objdump or nm.
/// @return E.g. "aarch64-linux-gnu-as".
std::string crossTool( const std::string &name );

/// @return "qemu-aarch64", its static build, or empty when neither is installed.
std::string qemuProgram();

/// @brief Builds an executable next to the source: .c with GCC (static glibc), else as
///        assembly.
/// @return Success, executable path and each step's messages.
BuildResult buildProgram( const std::string &sourcePath );

/// @brief Reads the DWARF line table.
/// @param[in] sourcePath Source whose lines are wanted.
/// @return Row address to line; 0 for code outside the source.
std::map<uint64_t, int> readSourceLines( const std::string &executablePath,
                                         const std::string &sourcePath );

/// @return Symbol address, or 0 if not defined.
uint64_t symbolAddress( const std::string &executablePath, const std::string &name );
