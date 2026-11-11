#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <core/process.h>

static void closePipe( int fds[2] ) {
    if ( fds[0] >= 0 ) {
        close( fds[0] );
        close( fds[1] );
    }
}

// The child gets its own process group, so stopping it also stops what it started.
static pid_t spawnProcess( const std::vector<std::string> &args, const std::string &directory,
                           int &outputFd, int *inputFd = nullptr ) {
    int pipeFds[2];
    int inputFds[2] = { -1, -1 };
    if ( pipe( pipeFds ) != 0 ) {
        return -1;
    }
    if ( inputFd && pipe( inputFds ) != 0 ) {
        closePipe( pipeFds );
        return -1;
    }

    pid_t pid = fork();
    if ( pid < 0 ) {
        closePipe( pipeFds );
        closePipe( inputFds );
        return -1;
    }

    if ( pid == 0 ) {
        setpgid( 0, 0 );
        signal( SIGPIPE, SIG_DFL );
        int stdinFd = inputFd ? inputFds[0] : open( "/dev/null", O_RDONLY );
        dup2( stdinFd, STDIN_FILENO );
        dup2( pipeFds[1], STDOUT_FILENO );
        dup2( pipeFds[1], STDERR_FILENO );
        if ( !inputFd ) {
            close( stdinFd );
        }
        closePipe( inputFds );
        closePipe( pipeFds );
        if ( !directory.empty() && chdir( directory.c_str() ) != 0 ) {
            dprintf( STDERR_FILENO, "Cannot enter %s\n", directory.c_str() );
            _exit( 127 );
        }

        std::vector<char *> argv;
        for ( const std::string &arg : args ) {
            argv.push_back( const_cast<char *>( arg.c_str() ) );
        }
        argv.push_back( nullptr );
        execvp( argv[0], argv.data() );
        dprintf( STDERR_FILENO, "Cannot run %s\n", argv[0] );
        _exit( 127 );
    }

    setpgid( pid, pid );
    close( pipeFds[1] );
    outputFd = pipeFds[0];
    if ( inputFd ) {
        close( inputFds[0] );
        *inputFd = inputFds[1];
    }
    return pid;
}

static void closeProcessPipes( Process &process ) {
    close( process.outputFd );
    if ( process.inputFd >= 0 ) {
        close( process.inputFd );
    }
    process.pid = -1;
    process.outputFd = -1;
    process.inputFd = -1;
}

Process startProcess( const std::vector<std::string> &args, const std::string &directory,
                      bool withInput ) {
    Process process;
    process.pid =
        spawnProcess( args, directory, process.outputFd, withInput ? &process.inputFd : nullptr );
    if ( isRunning( process ) ) {
        fcntl( process.outputFd, F_SETFL, O_NONBLOCK );
    }
    return process;
}

std::string pollProcess( Process &process ) {
    int status;
    bool exited = waitpid( process.pid, &status, WNOHANG ) == process.pid;

    std::string output;
    char buffer[4096];
    ssize_t count;
    while ( ( count = read( process.outputFd, buffer, sizeof( buffer ) ) ) > 0 ) {
        output.append( buffer, count );
    }

    if ( exited ) {
        closeProcessPipes( process );
        process.exitCode = WIFEXITED( status ) ? WEXITSTATUS( status ) : 0;
        process.signal = WIFSIGNALED( status ) ? WTERMSIG( status ) : 0;
    }
    return output;
}

// A process that has exited makes write fail with EPIPE instead of raising SIGPIPE.
bool writeToProcess( Process &process, const std::string &text ) {
    signal( SIGPIPE, SIG_IGN );
    return isRunning( process ) && process.inputFd >= 0 &&
           write( process.inputFd, text.data(), text.size() ) ==
               static_cast<ssize_t>( text.size() );
}

void stopProcess( Process &process ) {
    kill( -process.pid, SIGKILL );
    waitpid( process.pid, nullptr, 0 );
    closeProcessPipes( process );
    process.exitCode = 0;
    process.signal = SIGKILL;
}

bool isRunning( const Process &process ) {
    return process.pid > 0;
}

CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory ) {
    int outputFd;
    pid_t pid = spawnProcess( args, directory, outputFd );
    if ( pid < 0 ) {
        return { -1, "Cannot start " + args[0] + "\n" };
    }

    CommandResult result;
    char buffer[4096];
    ssize_t count;
    while ( ( count = read( outputFd, buffer, sizeof( buffer ) ) ) > 0 ) {
        result.output.append( buffer, count );
    }
    close( outputFd );

    int status;
    waitpid( pid, &status, 0 );
    result.exitCode = WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
    return result;
}
