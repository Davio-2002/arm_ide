#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <core/header_declarations.h>

/// @brief Completes AArch64 assembly words and the document's labels, ignoring case; an
///        upper-case term gets upper-case words.
/// @return Words longer than the term that start with it, shortest first.
std::vector<std::string> completeAssembly( const std::string &term,
                                           const std::vector<std::string> &documentWords );

/// @brief Completes C keywords, header functions and macros, and the document's identifiers.
/// @return Words longer than the term that start with it, shortest first.
std::vector<std::string> completeC( const std::string &term,
                                    const std::vector<Declaration> &declarations,
                                    const std::vector<std::string> &documentWords );

/// @brief Completes the directive after a line's leading '#', most used first, with the
///        space before its argument: "include ".
/// @return Nothing if the cursor is not in the directive word.
std::optional<std::vector<std::string>> completeDirective( const std::string &line, size_t column );

struct IncludeCompletion {
    size_t start = 0;
    size_t end = 0;
    std::vector<std::string> names;
};

/// @brief Completes an #include header name: .h files and folders of the compiler's include
///        folders for <...>, of the source's folder for "...".
/// @return The name's columns (what a pick replaces) and the paths, folders ending in '/';
///         nothing outside a header name.
std::optional<IncludeCompletion> completeInclude( const std::string &line, size_t column,
                                                  const std::string &directory );
