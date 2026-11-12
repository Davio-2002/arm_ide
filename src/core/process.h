#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

struct Process {
    pid_t pid = -1;
    int outputFd = -1;
    int inputFd = -1; // the program's stdin
    int exitCode = 0;
    int signal = 0;
};

struct CommandResult {
    int exitCode = -1;
    std::string output;
};

Process startProcess( const std::vector<std::string> &args );
std::string pollProcess( Process &process );
// False when the program is not reading and the pipe is full, or its input was closed.
bool writeToProcess( Process &process, const std::string &text );
// The program then reads end of file.
void closeProcessInput( Process &process );
void stopProcess( Process &process );
bool isRunning( const Process &process );

CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory );
