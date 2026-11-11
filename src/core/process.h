#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

struct Process {
    pid_t pid = -1;
    int outputFd = -1;
    int inputFd = -1; // only for processes started with input
    int exitCode = 0;
    int signal = 0;
};

struct CommandResult {
    int exitCode = -1;
    std::string output;
};

// Without input the process reads /dev/null.
Process startProcess( const std::vector<std::string> &args, const std::string &directory = "",
                      bool withInput = false );
std::string pollProcess( Process &process );
bool writeToProcess( Process &process, const std::string &text );
void stopProcess( Process &process );
bool isRunning( const Process &process );

CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory );
