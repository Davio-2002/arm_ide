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

enum class LineMarkKind { Error, Warning, Paused };

struct LineMark {
    int line = 0;
    LineMarkKind kind = LineMarkKind::Error;
    std::string message;
};

class Session {
  public:
    bool openFile( const std::string &path, std::string &text );
    bool saveFile( const std::string &path, const std::string &text );

    bool build();
    void run();
    void debugContinue();
    void debugStep();
    void stop();
    void toggleBreakpoint( const std::string &path, int line );
    // While a program runs, lines typed in the Console go to its stdin.
    void sendProgramInput( const std::string &line );
    void endProgramInput();
    void clearOutput();
    // Starts the shell when the Terminal tab is shown, and keeps the pty sized to the panel.
    void openTerminal( int rows, int columns );
    void sendTerminalInput( const std::vector<TerminalInput> &inputs, const std::string &paste );
    void stopTerminal();
    void clearTerminal();
    void update();

    bool isDebugging() const;
    bool isPaused() const;
    bool isProgramActive() const;
    bool isProgramBusy() const;
    bool readMemory( uint64_t address, uint8_t &byte );
    bool hasBreakpoint( const std::string &path, int line ) const;
    bool isTerminalBusy() const;
    bool acceptsInput() const;

    std::string filePath;
    // The file last built: lineMarks, lineToReveal and pausedLine are lines of this file.
    std::string programFilePath;
    std::string status;
    std::vector<std::string> recentFiles;
    // Program output and run results; the assembler and linker output goes to buildLog.
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
    void addRecentFile( const std::string &path );
    void setLineMarks( const std::vector<LineMark> &marks );
    void appendProgramOutput( const std::string &output );
    void printResult( const std::string &text, MessageKind kind, int sourceLine = 0 );
    void printExitCode( int exitCode );

    Process *programProcess();
    void stopActiveProgram();
    void startDebugging( bool pauseAtEntry );
    void endDebugSession();
    void handleDebuggerState();
    void showRunning();
    void showPausedLine();
    void showCrash();
    void updateRunningProgram();
    void updateDebugger();

    Debugger debugger;
    // Memory read while paused, in aligned blocks; an empty block could not be read.
    std::map<uint64_t, std::vector<uint8_t>> memoryBlocks;
    Process runningProgram;
    std::string executablePath;
    bool programLineOpen = false;
    std::chrono::steady_clock::time_point terminalActivity;
};
