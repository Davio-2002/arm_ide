#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <core/process.h>

static pid_t spawnProcess( const std::vector<std::string> &args, const std::string &directory,
                           int &outputFd ) {
    int pipeFds[2];
    if ( pipe( pipeFds ) != 0 ) {
        return -1;
    }

    pid_t pid = fork();
    if ( pid < 0 ) {
        close( pipeFds[0] );
        close( pipeFds[1] );
        return -1;
    }

    if ( pid == 0 ) {
        int nullFd = open( "/dev/null", O_RDONLY );
        dup2( nullFd, STDIN_FILENO );
        dup2( pipeFds[1], STDOUT_FILENO );
        dup2( pipeFds[1], STDERR_FILENO );
        close( nullFd );
        close( pipeFds[0] );
        close( pipeFds[1] );
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

    close( pipeFds[1] );
    outputFd = pipeFds[0];
    return pid;
}

Process startProcess( const std::vector<std::string> &args ) {
    Process process;
    process.pid = spawnProcess( args, "", process.outputFd );
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
