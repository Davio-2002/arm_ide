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

Process startProcess( const std::vector<std::string> &args );
std::string pollProcess( Process &process );
// Goes to the program's stdin. False when it is not reading and the terminal's buffer is full.
bool writeToProcess( Process &process, const std::string &text );
// The program's next read gets end of file, as after Ctrl+D in a terminal.
void endProcessInput( Process &process );
void stopProcess( Process &process );
bool isRunning( const Process &process );

// An executable file of that name in a PATH directory, or at that path when it has a slash.
bool isInPath( const std::string &program );

CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory );
