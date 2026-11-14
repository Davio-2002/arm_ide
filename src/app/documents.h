#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <core/header_declarations.h>

/// @brief An open or untitled file. It is modified while the frontend's edit version differs
///        from the one at the last open or save.
struct Document {
    int id = 0;
    std::string path;
    std::string diskText;
    size_t version = 0;
    std::optional<size_t> savedVersion;
    std::vector<Declaration> declarations;
    std::string declarationsSource;
};

/// @return True if it has unsaved edits.
bool isModified( const Document &document );

/// @return File name, or "untitled" for an empty path.
std::string documentName( const std::string &path );

/// @return The path with the home folder as "~".
std::string displayPath( const std::string &path );

/// @return Absolute path without "." and ".." parts, so a file has one name.
std::string absolutePath( const std::string &path );

/// @return $XDG_CONFIG_HOME/arm_ide (default ~/.config/arm_ide), created if missing.
std::filesystem::path configDirectory();

/// @return Saved recent files that still exist, newest first.
std::vector<std::string> readRecentFiles();

/// @param[in] paths Absolute paths, newest first.
void writeRecentFiles( const std::vector<std::string> &paths );
