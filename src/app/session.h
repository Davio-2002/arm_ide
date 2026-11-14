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

/// @brief Kind of a mark the editor shows next to a line.
enum class LineMarkKind { Error, Warning, Paused };

/// @brief A mark on one line of the built file: a build message or the paused line.
struct LineMark {
    int line = 0;
    LineMarkKind kind = LineMarkKind::Error;
    std::string message;
};

/// @brief The program as the debugger last saw it paused.
struct PausedState {
    /// True once the debugger paused; the registers stay shown after the session ends.
    bool hasRegisters = false;
    Registers registers;
    /// Registers at the pause before, to show what changed.
    Registers previousRegisters;
    /// Line of the built file the program is paused on; 0 while not paused or outside it.
    int line = 0;
};

/// @brief State of the IDE behind any frontend: open documents, builds, running and debugging
///        the program.
///
/// A frontend reads the public fields and the const methods to draw, and calls the other
/// methods for user actions, never while it is drawing. It never draws or reads input itself.
/// The document methods are defined in documents.cpp, the rest in session.cpp.
class Session {
  public:
    /// @brief Starts with one empty untitled document and the saved recent files.
    Session();

    // Commands ------------------------------------------------------------------------------

    /// @return Its name for menus and tooltips, such as "Build and Run".
    std::string commandLabel( Command command ) const;

    /// @return False if the command does nothing now, as Stop without a program.
    bool canRun( Command command ) const;

    /// @return The file the frontend must ask for before running the command: one to open,
    ///         or a name to save under (Save As, or a build of an untitled document).
    PathRequest pathNeeded( Command command ) const;

    /// @brief Runs a command. Build, Run and starting the debugger save the active document
    ///        first and stop if it can't be saved.
    /// @param[in] input   Active document text, cursor line, and the path pathNeeded() asked for.
    void run( Command command, const CommandInput &input );

    // Documents -----------------------------------------------------------------------------

    /// @return Open documents, in tab order. There is always at least one.
    const std::vector<Document> &documents() const;

    /// @return The document Build, Run and Save work on.
    const Document &activeDocument() const;

    /// @brief Opens a file, or activates its document if already open. An empty, unmodified
    ///        untitled active document is replaced.
    /// @param[in] path File to open.
    /// @return True if the file is open; otherwise @ref status holds the error.
    bool openFile( const std::string &path );

    /// @param[in] id Document to make active; ignored if not open.
    void activateDocument( int id );

    /// @brief Closes a document without asking. The last one is replaced by an untitled one.
    /// @param[in] id Document to close.
    void closeDocument( int id );

    /// @brief Records the frontend's edit version of a document's text.
    /// @param[in] version Edit version, equal to an earlier one when the text is the same again.
    void setDocumentVersion( int id, size_t version );

    /// @brief Reads a C document's header declarations again from its edited text, if its
    ///        preprocessor lines changed since they were read.
    /// @param[in] text Text being edited.
    void updateDeclarations( int id, const std::string &text );

    /// @return Names of all documents with unsaved changes.
    std::vector<std::string> unsavedDocumentNames() const;

    /// @brief Activates the built file and asks the frontend to show a line of it.
    /// @param[in] line 1-based line of the built file.
    void showProgramLine( int line );

    /// @brief Takes the line the frontend should scroll to in the active document.
    /// @return 1-based line, or 0 if none was asked for since the last call.
    int takeLineToReveal();

    // Program -------------------------------------------------------------------------------

    /// @brief Adds or removes a breakpoint. Applies it at once if the file is being debugged.
    /// @param[in] path Absolute path of the file.
    /// @param[in] line 1-based line number.
    void toggleBreakpoint( const std::string &path, int line );

    /// @param[in] path Absolute path of the file.
    /// @param[in] line 1-based line number.
    /// @return True if the line has a breakpoint.
    bool hasBreakpoint( const std::string &path, int line ) const;

    /// @brief Sends a line typed by the user to the stdin of the running program.
    ///
    /// The line is echoed into the console, as a terminal does.
    /// @param[in] line Typed text, without the trailing newline.
    void sendProgramInput( const std::string &line );

    /// @brief Sends end of file to the program's stdin, as Ctrl+D does in a terminal.
    void endProgramInput();

    /// @return True if a program can read input.
    bool acceptsInput() const;

    /// @brief Reads one byte of program memory while the debugger is paused.
    ///
    /// Memory is fetched in aligned blocks and cached until the program resumes.
    /// @param[in]  address Address in the program.
    /// @param[out] byte    Byte at @p address.
    /// @return True if the byte was read; false if not paused or the memory is unreadable.
    bool readMemory( uint64_t address, uint8_t &byte );

    /// @brief Collects output and state changes of the program and the debugger. Called once
    ///        per frame or loop iteration.
    void update();

    /// @return True while a debug session is active, paused or running.
    bool isDebugging() const;

    /// @return True while the debugger is paused.
    bool isPaused() const;

    /// @return True while a program runs, with or without the debugger.
    bool isProgramActive() const;

    /// @return True while a program is executing, that is, running and not paused.
    bool isProgramBusy() const;

    /// @brief Ends the program, if any. Called when the IDE quits.
    void stop();

    /// File last built; the line marks, paused line and build messages refer to it.
    std::string programFilePath;
    std::string status;
    std::vector<std::string> recentFiles;
    std::vector<Message> console;
    std::vector<Message> buildLog;
    /// Counts builds, so a frontend can react once to each.
    int buildCount = 0;
    bool buildSucceeded = false;

    std::vector<LineMark> lineMarks;
    /// Changes whenever @ref lineMarks change.
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
