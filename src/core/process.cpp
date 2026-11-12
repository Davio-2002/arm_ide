#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <core/process.h>

// With inputFd the child reads a pipe whose write end is returned there, else /dev/null.
// The pipes are close-on-exec, so other children (the terminal's shell) never hold them open.
static pid_t spawnProcess( const std::vector<std::string> &args, const std::string &directory,
                           int &outputFd, int *inputFd ) {
    int outputPipe[2];
    if ( pipe2( outputPipe, O_CLOEXEC ) != 0 ) {
        return -1;
    }
    int inputPipe[2] = { -1, -1 };
    if ( inputFd && pipe2( inputPipe, O_CLOEXEC ) != 0 ) {
        close( outputPipe[0] );
        close( outputPipe[1] );
        return -1;
    }

    pid_t pid = fork();
    if ( pid < 0 ) {
        for ( int fd : { outputPipe[0], outputPipe[1], inputPipe[0], inputPipe[1] } ) {
            if ( fd >= 0 ) {
                close( fd );
            }
        }
        return -1;
    }

    if ( pid == 0 ) {
        signal( SIGPIPE, SIG_DFL );
        int stdinFd = inputFd ? inputPipe[0] : open( "/dev/null", O_RDONLY );
        dup2( stdinFd, STDIN_FILENO );
        dup2( outputPipe[1], STDOUT_FILENO );
        dup2( outputPipe[1], STDERR_FILENO );
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

    close( outputPipe[1] );
    outputFd = outputPipe[0];
    if ( inputFd ) {
        close( inputPipe[0] );
        *inputFd = inputPipe[1];
    }
    return pid;
}

Process startProcess( const std::vector<std::string> &args ) {
    // writeToProcess after the program exited must fail with EPIPE, not kill the IDE.
    signal( SIGPIPE, SIG_IGN );
    Process process;
    process.pid = spawnProcess( args, "", process.outputFd, &process.inputFd );
    if ( isRunning( process ) ) {
        fcntl( process.outputFd, F_SETFL, O_NONBLOCK );
        fcntl( process.inputFd, F_SETFL, O_NONBLOCK );
    }
    return process;
}

bool writeToProcess( Process &process, const std::string &text ) {
    size_t written = 0;
    while ( process.inputFd >= 0 && written < text.size() ) {
        ssize_t count = write( process.inputFd, text.data() + written, text.size() - written );
        if ( count <= 0 ) {
            return false;
        }
        written += count;
    }
    return written == text.size();
}

void closeProcessInput( Process &process ) {
    if ( process.inputFd >= 0 ) {
        close( process.inputFd );
        process.inputFd = -1;
    }
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
        closeProcessInput( process );
        close( process.outputFd );
        process.pid = -1;
        process.outputFd = -1;
        process.exitCode = WIFEXITED( status ) ? WEXITSTATUS( status ) : 0;
        process.signal = WIFSIGNALED( status ) ? WTERMSIG( status ) : 0;
    }
    return output;
}

void stopProcess( Process &process ) {
    kill( process.pid, SIGKILL );
    waitpid( process.pid, nullptr, 0 );
    closeProcessInput( process );
    close( process.outputFd );
    process.pid = -1;
    process.outputFd = -1;
    process.exitCode = 0;
    process.signal = SIGKILL;
}

bool isRunning( const Process &process ) {
    return process.pid > 0;
}

CommandResult runCommand( const std::vector<std::string> &args, const std::string &directory ) {
    int outputFd;
    pid_t pid = spawnProcess( args, directory, outputFd, nullptr );
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
