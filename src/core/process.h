#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

/// @brief A child process started with startProcess().
struct Process {
    pid_t pid = -1;
    int outputFd = -1;
    int exitCode = 0;
    int signal = 0;
};

/// @brief Result of runCommand().
struct CommandResult {
    int exitCode = -1;
    std::string output;
};

/// @brief Starts a program on a pseudo-terminal without waiting for it.
///
/// On a pty glibc flushes printf at each line and before reading stdin, as in a terminal.
/// @param[in] args Program and its arguments; the program is looked up in PATH.
/// @return The started process; not running if it could not start.
Process startProcess( const std::vector<std::string> &args );

/// @brief Reads available output without blocking and notices if the process ended.
/// @param[in,out] process Running process; reset with its exit status once it ended.
/// @return Output since the last call.
std::string pollProcess( Process &process );

/// @brief Writes text to the program's stdin.
/// @param[in,out] process Running process.
/// @param[in]     text    Text to send.
/// @return False when the program is not reading and the terminal's buffer is full.
bool writeToProcess( Process &process, const std::string &text );

/// @brief Makes the program's next read get end of file, as after Ctrl+D in a terminal.
/// @param[in,out] process Running process.
void endProcessInput( Process &process );

/// @brief Kills the process and waits for it to end.
/// @param[in,out] process Running process; reset as ended by SIGKILL.
void stopProcess( Process &process );

/// @param[in] process Process to check.
/// @return True until the process ended and pollProcess() or stopProcess() noticed it.
bool isRunning( const Process &process );

/// @brief Tells if a program can be run.
/// @param[in] program Program name searched in PATH, or a path if it contains a slash.
/// @return True if an executable file is found.
bool isInPath( const std::string &program );

/// @brief Runs a command to the end and collects its output.
/// @param[in] args      Program and its arguments; the program is looked up in PATH.
/// @param[in] directory Working directory; empty for the current one.
/// @return Exit code and combined output.
CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory );
