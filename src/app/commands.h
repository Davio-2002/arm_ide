#pragma once

#include <cstddef>
#include <string>

/// @brief A user command, run by Session::run(). A new one needs its cases in the switches
///        of session.cpp.
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

enum class PathRequest { None, FileToOpen, FileToSave };

/// @brief What a command needs from the frontend, which owns the text buffers.
struct CommandInput {
    std::string text;
    size_t version = 0;
    int line = 0;
    std::string path;
};
