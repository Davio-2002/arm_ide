#pragma once

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <core/debugger.h>
#include <core/process.h>
#include <core/terminal.h>
#include <core/toolchain.h>

/// @brief Kind of a mark the editor shows next to a line.
enum class LineMarkKind { Error, Warning, Paused };

/// @brief A mark on one line of the built file: a build message or the paused line.
struct LineMark {
    int line = 0;
    LineMarkKind kind = LineMarkKind::Error;
    std::string message;
};

/// @brief State of the IDE behind the GUI: the open file, builds, running and debugging the
///        program, and the terminal.
///
/// The GUI reads the public fields to draw and calls the methods for user actions.
class Session {
  public:
    /// @brief Reads a file and makes it the current file.
    /// @param[in]  path File to open.
    /// @param[out] text File contents.
    /// @return True if the file was read; otherwise @ref status holds the error.
    bool openFile( const std::string &path, std::string &text );

    /// @brief Writes text to a file and makes it the current file.
    ///
    /// Breakpoints follow the file when it is saved under a new name.
    /// @param[in] path File to write.
    /// @param[in] text New file contents.
    /// @return True if the file was written; otherwise @ref status holds the error.
    bool saveFile( const std::string &path, const std::string &text );

    /// @brief Stops any running program and builds the current file.
    ///
    /// Fills @ref buildLog and the line marks with the build messages.
    /// @return True if the build produced an executable.
    bool build();

    /// @brief Builds the current file and runs it in QEMU without the debugger.
    void run();

    /// @brief Starts debugging, or resumes the program if it is paused.
    void debugContinue();

    /// @brief Starts debugging paused at the first line, or steps to the next line if paused.
    void debugStep();

    /// @brief Stops the running or debugged program.
    void stop();

    /// @brief Adds or removes a breakpoint. Applies it at once if the file is being debugged.
    /// @param[in] path Absolute path of the file.
    /// @param[in] line 1-based line number.
    void toggleBreakpoint( const std::string &path, int line );

    /// @brief Sends a line typed in the Console to the stdin of the running program.
    ///
    /// The line is echoed into the console, as a terminal does.
    /// @param[in] line Typed text, without the trailing newline.
    void sendProgramInput( const std::string &line );

    /// @brief Sends end of file to the program's stdin, as Ctrl+D does in a terminal.
    void endProgramInput();

    /// @brief Clears the console and the build log.
    void clearOutput();

    /// @brief Starts the shell when the Terminal tab is first shown and keeps the pty sized
    ///        to the panel.
    /// @param[in] rows    Visible rows of the panel.
    /// @param[in] columns Visible columns of the panel.
    void openTerminal( int rows, int columns );

    /// @brief Sends keys and pasted text to the shell. A key press after the shell exited
    ///        starts a new shell.
    /// @param[in] inputs Key presses and typed characters, in order.
    /// @param[in] paste  Text to paste; may be empty.
    void sendTerminalInput( const std::vector<TerminalInput> &inputs, const std::string &paste );

    /// @brief Ends the shell and all programs running in it.
    void stopTerminal();

    /// @brief Clears the scrollback and sends Ctrl+L to the shell.
    void clearTerminal();

    /// @brief Collects output and state changes of the program, debugger and terminal.
    ///        Called once per frame.
    void update();

    /// @return True while a debug session is active, paused or running.
    bool isDebugging() const;

    /// @return True while the debugger is paused.
    bool isPaused() const;

    /// @return True while a program runs, with or without the debugger.
    bool isProgramActive() const;

    /// @return True while a program is executing, that is, running and not paused.
    bool isProgramBusy() const;

    /// @brief Reads one byte of program memory while the debugger is paused.
    ///
    /// Memory is fetched in aligned blocks and cached until the program resumes.
    /// @param[in]  address Address in the program.
    /// @param[out] byte    Byte at @p address.
    /// @return True if the byte was read; false if not paused or the memory is unreadable.
    bool readMemory( uint64_t address, uint8_t &byte );

    /// @param[in] path Absolute path of the file.
    /// @param[in] line 1-based line number.
    /// @return True if the line has a breakpoint.
    bool hasBreakpoint( const std::string &path, int line ) const;

    /// @brief Tells if the terminal had input or output in the last moment.
    ///
    /// Output often follows input by a few milliseconds, so the frame loop stays fast for
    /// a while after any activity.
    /// @return True within 500 ms of the last terminal activity.
    bool isTerminalBusy() const;

    /// @return True if a program can read input typed in the Console.
    bool acceptsInput() const;

    std::string filePath;
    std::string programFilePath;
    std::string status;
    std::vector<std::string> recentFiles;
    std::vector<Message> console;
    std::vector<Message> buildLog;
    Terminal terminal;
    int buildCount = 0;
    bool buildSucceeded = false;

    std::vector<LineMark> lineMarks;
    int lineMarksVersion = 0;
    int lineToReveal = 0;

    std::map<std::string, std::set<int>> breakpoints;
    int pausedLine = 0;
    Registers registers;
    Registers previousRegisters;
    bool hasRegisters = false;

  private:
    /// @brief Moves a file to the top of @ref recentFiles.
    /// @param[in] path File path; may be an element of @ref recentFiles.
    void addRecentFile( const std::string &path );

    /// @brief Replaces the line marks and bumps @ref lineMarksVersion.
    /// @param[in] marks New marks.
    void setLineMarks( const std::vector<LineMark> &marks );

    /// @brief Appends program output to the console, splitting it into lines.
    /// @param[in] output Raw output; may end in the middle of a line.
    void appendProgramOutput( const std::string &output );

    /// @brief Prints a result line to the console and shows it in the status bar.
    /// @param[in] text       Message text.
    /// @param[in] kind       Message colour.
    /// @param[in] sourceLine Line the message refers to; 0 for none.
    void printResult( const std::string &text, MessageKind kind, int sourceLine = 0 );

    /// @brief Prints the program's exit code, as success for 0 and as error otherwise.
    /// @param[in] exitCode Exit code of the program.
    void printExitCode( int exitCode );

    /// @return The process reading program input, or nullptr if no program runs.
    Process *programProcess();

    /// @brief Stops the running program and ends the debug session, if any.
    void stopActiveProgram();

    /// @brief Builds the current file and starts it under the debugger.
    /// @param[in] pauseAtEntry True to pause at the first line, false to run to a breakpoint.
    void startDebugging( bool pauseAtEntry );

    /// @brief Stops the debugger and clears the paused line.
    void endDebugSession();

    /// @brief Collects program output and shows the debugger's new state.
    void handleDebuggerState();

    /// @brief Clears the paused line when a step leaves the program running, for example
    ///        while it waits in read for input.
    void showRunning();

    /// @brief Reads the registers and marks the line the debugger is paused on.
    void showPausedLine();

    /// @brief Ends the debug session and reports the crash with its line, if known.
    void showCrash();

    /// @brief Collects output of the program run without the debugger and reports its exit.
    void updateRunningProgram();

    /// @brief Handles stop replies of a program running under the debugger.
    void updateDebugger();

    Debugger debugger;
    std::map<uint64_t, std::vector<uint8_t>> memoryBlocks;
    Process runningProgram;
    std::string executablePath;
    bool programLineOpen = false;
    std::chrono::steady_clock::time_point terminalActivity;
};
