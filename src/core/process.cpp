#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <pty.h>
#include <sstream>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <core/process.h>

namespace {

/// @brief Opens the child's output channel: a pty for programs (canonical, no echo, no output
///        translation), a pipe and /dev/null for tools.
/// @param[out] fds Parent end in fds[0], child end in fds[1]; both close-on-exec.
/// @return True if opened.
bool openOutput( bool isProgram, std::array<int, 2> &fds ) {
    if ( !isProgram ) {
        return pipe2( fds.data(), O_CLOEXEC ) == 0;
    }
    if ( openpty( fds.data(), &fds[1], nullptr, nullptr, nullptr ) != 0 ) {
        return false;
    }
    termios settings;
    tcgetattr( fds[1], &settings );
    cfmakeraw( &settings );
    settings.c_lflag |= ICANON;
    tcsetattr( fds[1], TCSANOW, &settings );
    fcntl( fds[0], F_SETFD, FD_CLOEXEC );
    fcntl( fds[1], F_SETFD, FD_CLOEXEC );
    return true;
}

/// @brief Forks and runs a program with its output going to the parent.
/// @return Child pid, or -1 on failure.
pid_t spawnProcess( const std::vector<std::string> &args, const std::string &directory,
                    int &outputFd, bool isProgram ) {
    std::array<int, 2> output{};
    if ( !openOutput( isProgram, output ) ) {
        return -1;
    }

    pid_t const pid = fork();
    if ( pid < 0 ) {
        close( output[0] );
        close( output[1] );
        return -1;
    }

    if ( pid == 0 ) {
        int const inputFd = isProgram ? output[1] : open( "/dev/null", O_RDONLY );
        if ( inputFd < 0 ) {
            _exit( 127 );
        }
        dup2( inputFd, STDIN_FILENO );
        dup2( output[1], STDOUT_FILENO );
        dup2( output[1], STDERR_FILENO );
        if ( !directory.empty() && chdir( directory.c_str() ) != 0 ) {
            dprintf( STDERR_FILENO, "Cannot enter %s\n", directory.c_str() );
            _exit( 127 );
        }

        std::vector<char *> argv;
        argv.reserve( args.size() + 1 );
        for ( const std::string &arg : args ) {
            argv.push_back( const_cast<char *>( arg.c_str() ) );
        }
        argv.push_back( nullptr );
        execvp( argv[0], argv.data() );
        dprintf( STDERR_FILENO, "Cannot run %s\n", argv[0] );
        _exit( 127 );
    }

    close( output[1] );
    outputFd = output[0];
    return pid;
}

} // anonymous namespace

Process startProcess( const std::vector<std::string> &args ) {
    Process process;
    process.pid = spawnProcess( args, "", process.outputFd, true );
    if ( isRunning( process ) ) {
        fcntl( process.outputFd, F_SETFL, O_NONBLOCK );
    }
    return process;
}

bool writeToProcess( Process &process, const std::string &text ) {
    size_t written = 0;
    while ( isRunning( process ) && written < text.size() ) {
        ssize_t const count =
            write( process.outputFd, text.data() + written, text.size() - written );
        if ( count <= 0 ) {
            return false;
        }
        written += count;
    }
    return written == text.size();
}

void endProcessInput( Process &process ) {
    writeToProcess( process, "\x04" );
}

std::string pollProcess( Process &process ) {
    int status;
    bool const exited = waitpid( process.pid, &status, WNOHANG ) == process.pid;

    std::string output;
    std::array<char, 4096> buffer{};
    ssize_t count;
    while ( ( count = read( process.outputFd, buffer.data(), buffer.size() ) ) > 0 ) {
        output.append( buffer.data(), count );
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
    pid_t const pid = spawnProcess( args, directory, outputFd, false );
    if ( pid < 0 ) {
        return { -1, "Cannot start " + args[0] + "\n" };
    }

    CommandResult result;
    std::array<char, 4096> buffer{};
    ssize_t count;
    while ( ( count = read( outputFd, buffer.data(), buffer.size() ) ) > 0 ) {
        result.output.append( buffer.data(), count );
    }
    close( outputFd );

    int status;
    waitpid( pid, &status, 0 );
    result.exitCode = WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
    return result;
}

bool isInPath( const std::string &program ) {
    auto isExecutable = []( const std::filesystem::path &path ) {
        std::error_code error;
        return std::filesystem::is_regular_file( path, error ) && access( path.c_str(), X_OK ) == 0;
    };
    if ( program.contains( '/' ) ) {
        return isExecutable( program );
    }
    const char *path = std::getenv( "PATH" );
    std::istringstream directories( path ? path : "" );
    std::string directory;
    while ( std::getline( directories, directory, ':' ) ) {
        if ( !directory.empty() && isExecutable( std::filesystem::path( directory ) / program ) ) {
            return true;
        }
    }
    return false;
}
