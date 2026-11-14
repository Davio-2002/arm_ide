#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include <app/commands.h>
#include <app/documents.h>
#include <core/debugger.h>
#include <core/process.h>
#include <core/toolchain.h>

enum class LineMarkKind { Error, Warning, Paused };

/// @brief A build message or the paused line, on a line of the built file.
struct LineMark {
    int line = 0;
    LineMarkKind kind = LineMarkKind::Error;
    std::string message;
};

struct PausedState {
    bool hasRegisters = false;
    Registers registers;
    Registers previousRegisters;
    int line = 0;
};

/// @brief IDE state behind any frontend: documents, builds, running and debugging.
///
/// Frontends draw from public fields and const methods, and call the rest for user actions,
/// never while drawing. Document methods live in documents.cpp.
class Session {
  public:
    /// @brief Starts with one untitled document and the saved recent files.
    Session();

    // Commands

    /// @return Menu and tooltip name, e.g. "Build and Run".
    std::string commandLabel( Command command ) const;

    /// @return False if the command does nothing now, e.g. Stop without a program.
    bool canRun( Command command ) const;

    /// @return The file to ask for first: one to open, or a name to save under.
    PathRequest pathNeeded( Command command ) const;

    /// @brief Runs a command. Build, Run and Debug save the active document first and stop if
    ///        that fails.
    void run( Command command, const CommandInput &input );

    // Documents

    /// @return Open documents in tab order; never empty.
    const std::vector<Document> &documents() const;

    /// @return The document Build, Run and Save work on.
    const Document &activeDocument() const;

    /// @brief Opens a file or activates its document; replaces an empty untitled one.
    /// @return True if open; otherwise `status` says why.
    bool openFile( const std::string &path );

    /// @param[in] id Ignored if not open.
    void activateDocument( int id );

    /// @brief Closes without asking; the last one is replaced by an untitled one.
    void closeDocument( int id );

    /// @brief Records the frontend's edit version; equal versions mean equal text.
    void setDocumentVersion( int id, size_t version );

    /// @brief Rereads a C document's header declarations if its preprocessor lines changed.
    /// @param[in] text Edited text.
    void updateDeclarations( int id, const std::string &text );

    /// @return Names of documents with unsaved changes.
    std::vector<std::string> unsavedDocumentNames() const;

    /// @brief Activates the built file and asks the frontend to reveal a line.
    void showProgramLine( int line );

    /// @return 1-based line to scroll to, or 0 if none was asked since the last call.
    int takeLineToReveal();

    // Program

    /// @brief Applies at once if the file is being debugged.
    void toggleBreakpoint( const std::string &path, int line );

    bool hasBreakpoint( const std::string &path, int line ) const;

    /// @brief Sends a line to the program's stdin and echoes it into the console.
    /// @param[in] line Text without the newline.
    void sendProgramInput( const std::string &line );

    /// @brief Sends end of file, like Ctrl+D.
    void endProgramInput();

    /// @return True if a program can read input.
    bool acceptsInput() const;

    /// @brief Reads a byte while paused; aligned blocks are cached until the program resumes.
    /// @return False if not paused or unreadable.
    bool readMemory( uint64_t address, uint8_t &byte );

    /// @brief Polls the program and the debugger; called once per frame.
    void update();

    /// @return True while a debug session is paused or running.
    bool isDebugging() const;

    bool isPaused() const;

    /// @return True while a program runs, with or without the debugger.
    bool isProgramActive() const;

    /// @return True while a program runs and is not paused.
    bool isProgramBusy() const;

    /// @brief Ends the program, if any; called on quit.
    void stop();

    std::string programFilePath;
    std::string status;
    std::vector<std::string> recentFiles;
    std::vector<Message> console;
    std::vector<Message> buildLog;
    int buildCount = 0;
    bool buildSucceeded = false;

    std::vector<LineMark> lineMarks;
    int lineMarksVersion = 0;

    std::map<std::string, std::set<int>> breakpoints;
    PausedState paused;

  private:
    Document *findDocument( int id );
    Document &activeDocumentForEdit();
    int addDocument( const std::string &path, const std::string &text );
    bool saveActiveDocument( const std::string &path, const CommandInput &input );
    bool saveBeforeBuild( const CommandInput &input );
    void addRecentFile( const std::string &path );
    void removeRecentFile( const std::string &path );

    bool build();
    void runProgram();
    void debugContinue();
    void debugStep();
    void clearOutput();

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

    std::vector<Document> openDocuments;
    int activeDocumentId = 0;
    int nextDocumentId = 1;
    int lineToReveal = 0;

    Debugger debugger;
    std::map<uint64_t, std::vector<uint8_t>> memoryBlocks;
    Process runningProgram;
    std::string executablePath;
    bool programLineOpen = false;
};
