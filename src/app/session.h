#pragma once

#include <set>
#include <string>
#include <vector>

#include <core/emulator.h>
#include <core/process.h>
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
    void toggleBreakpoint( int line );
    void update();

    bool isDebugging() const;
    bool isPaused() const;
    bool isProgramActive() const;
    bool isProgramBusy() const;
    bool readMemory( uint64_t address, uint8_t &byte ) const;

    std::string filePath;
    std::string status;
    std::vector<std::string> recentFiles;
    std::vector<Message> console;

    std::vector<LineMark> lineMarks;
    int lineMarksVersion = 0;
    int lineToReveal = 0;

    std::set<int> breakpoints;
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

    void stopActiveProgram();
    void startDebugging( bool pauseAtEntry );
    void endDebugSession();
    void handleEmulatorState();
    void showPausedLine();
    void showCrash();
    void updateRunningProgram();
    void updateEmulator();

    Emulator emulator;
    Process runningProgram;
    std::string executablePath;
    bool programLineOpen = false;
};
