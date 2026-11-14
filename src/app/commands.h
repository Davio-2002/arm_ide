#pragma once

#include <cstddef>
#include <string>

/// @brief A user command, the same in every frontend. Session::run() carries it out.
///
/// To add one: a value here, then its label, availability, path request and action in the
/// switches of session.cpp (commandLabel, canRun, pathNeeded, run).
enum class Command {
    NewFile,
    Open,
    Save,
    SaveAs,
    Build,
    Run,
    Stop,
    DebugContinue,
    DebugStep,
    ToggleBreakpoint,
    ClearOutput,
};

/// @brief What the frontend must ask the user before a command can run.
enum class PathRequest { None, FileToOpen, FileToSave };

/// @brief What a command needs from the frontend, which owns the text buffers.
struct CommandInput {
    /// Text of the active document; saved before a build.
    std::string text;
    /// Edit version of that text, as given to Session::setDocumentVersion().
    size_t version = 0;
    /// 1-based cursor line in the active document.
    int line = 0;
    /// File the user chose, for a command whose PathRequest is not None.
    std::string path;
};
