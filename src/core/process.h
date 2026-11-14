#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

struct Process {
    pid_t pid = -1;
    int outputFd = -1;
    int exitCode = 0;
    int signal = 0;
};

struct CommandResult {
    int exitCode = -1;
    std::string output;
};

/// @brief Starts a program on a pty, so glibc flushes printf per line as in a terminal.
/// @param[in] args Program and arguments, looked up in PATH.
/// @return The process; not running if it failed to start.
Process startProcess( const std::vector<std::string> &args );

/// @brief Reads available output without blocking.
/// @param[in,out] process Reset with its exit status once it ended.
/// @return Output since the last call.
std::string pollProcess( Process &process );

/// @return False when the program isn't reading and the pty buffer is full.
bool writeToProcess( Process &process, const std::string &text );

/// @brief Makes the next read get end of file, like Ctrl+D.
void endProcessInput( Process &process );

/// @brief Kills the process and waits for it.
void stopProcess( Process &process );

/// @return True until pollProcess() or stopProcess() noticed the end.
bool isRunning( const Process &process );

/// @param[in] program Name searched in PATH, or a path if it contains '/'.
/// @return True if an executable is found.
bool isInPath( const std::string &program );

/// @brief Runs a command to the end.
/// @param[in] args      Program and arguments, looked up in PATH.
/// @param[in] directory Working directory; empty for the current one.
/// @return Exit code and combined output.
CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory );
