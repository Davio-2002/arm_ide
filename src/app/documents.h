#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <core/header_declarations.h>

/// @brief One open file, or an untitled one.
///
/// The frontend keeps the text being edited, one buffer per document id, and reports an edit
/// version of it (an undo index): the document is modified while that version differs from
/// the one at the last open or save.
struct Document {
    int id = 0;
    /// Absolute, normalized path; empty for an untitled document.
    std::string path;
    /// Text as read from the file or last written to it; a new buffer starts with it.
    std::string diskText;
    /// Edit version last reported by the frontend.
    size_t version = 0;
    /// Version at the last open or save. Unset until the frontend reports the first version,
    /// which then becomes the saved one.
    std::optional<size_t> savedVersion;
    /// Functions and macros of the headers a C document includes, as of the last open or save.
    std::vector<Declaration> declarations;
    /// Path and preprocessor lines the declarations were read from, to read them only again
    /// when these change.
    std::string declarationsSource;
};

/// @param[in] document Document to check.
/// @return True if it has edits that are not saved.
bool isModified( const Document &document );

/// @param[in] path File path; may be empty.
/// @return File name without directory, or "untitled" for an empty path.
std::string documentName( const std::string &path );

/// @brief Shortens a path for display.
/// @param[in] path Absolute path.
/// @return "~/..." for paths under the home folder, the path itself otherwise.
std::string displayPath( const std::string &path );

/// @brief Makes a path absolute and removes "." and ".." parts, so one file has one name.
/// @param[in] path Relative or absolute path.
/// @return Normalized absolute path.
std::string absolutePath( const std::string &path );

/// @return Directory of the IDE's settings files: $XDG_CONFIG_HOME/arm_ide, by default
///         ~/.config/arm_ide. It is created if missing.
std::filesystem::path configDirectory();

/// @return The recent files saved by writeRecentFiles() that still exist, newest first.
std::vector<std::string> readRecentFiles();

/// @brief Saves the recent files in the config directory.
/// @param[in] paths Absolute paths, newest first.
void writeRecentFiles( const std::vector<std::string> &paths );
