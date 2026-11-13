#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/// @brief How a console or build log line is coloured.
enum class MessageKind { Output, Command, Error, Warning, Success };

/// @brief One line of the console or the build log.
struct Message {
    std::string text;
    MessageKind kind = MessageKind::Output;
    int sourceLine = 0;
};

/// @brief Result of buildProgram().
struct BuildResult {
    bool success = false;
    std::vector<Message> messages;
    std::string executablePath;
};

inline const std::string qemuNotFound =
    "qemu-aarch64 not found in PATH: install QEMU user-mode emulation (qemu-user)";

/// @brief Names an AArch64 cross tool as installed on this system.
///
/// The target prefix of the AArch64 binutils differs between distributions.
/// @param[in] name Tool name: as, ld, gcc, objdump or nm.
/// @return Prefixed tool name, for example "aarch64-linux-gnu-as".
std::string crossTool( const std::string &name );

/// @return "qemu-aarch64" or its static build, or an empty string when neither is installed.
std::string qemuProgram();

/// @brief Builds an executable next to the source file.
///
/// A .c file is compiled with GCC and glibc linked statically. Any other file is assembled
/// and linked as AArch64 assembly.
/// @param[in] sourcePath Source file to build.
/// @return Success, executable path and the messages of each step.
BuildResult buildProgram( const std::string &sourcePath );

/// @brief Reads the DWARF line table of an executable.
/// @param[in] executablePath Executable built with debug info.
/// @param[in] sourcePath     Source file whose lines are wanted.
/// @return Address of each line-table row to its line in @p sourcePath; 0 for code outside it.
std::map<uint64_t, int> readSourceLines( const std::string &executablePath,
                                         const std::string &sourcePath );

/// @brief Looks up a symbol in an executable.
/// @param[in] executablePath Executable to search.
/// @param[in] name           Symbol name, for example "main".
/// @return Symbol address, or 0 when the executable does not define it.
uint64_t symbolAddress( const std::string &executablePath, const std::string &name );
