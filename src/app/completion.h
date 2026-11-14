#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <core/header_declarations.h>

/// @brief Completes a word of AArch64 assembly: mnemonics, conditional branches, directives,
///        registers, operand keywords and the document's labels.
///
/// Case is ignored, and a term typed in upper case gets the assembly words in upper case.
/// @param[in] term          Typed start of a word.
/// @param[in] documentWords Identifiers found in the document.
/// @return Words longer than the term that start with it, shortest first.
std::vector<std::string> completeAssembly( const std::string &term,
                                           const std::vector<std::string> &documentWords );

/// @brief Completes a word of C: keywords, the functions and macros of the included headers
///        and the document's identifiers.
/// @param[in] term          Typed start of a word; matched ignoring case.
/// @param[in] declarations  Declarations of the headers the document includes.
/// @param[in] documentWords Identifiers found in the document.
/// @return Words longer than the term that start with it, shortest first.
std::vector<std::string> completeC( const std::string &term,
                                    const std::vector<Declaration> &declarations,
                                    const std::vector<std::string> &documentWords );

/// @brief Completes the directive word after the '#' that starts a C line, most used first.
///
/// The text to insert ends with the space before the argument: "include ", "define ", "endif".
/// @param[in] line   Text of the line.
/// @param[in] column Cursor column in @p line.
/// @return Nothing if the cursor is not in the directive word of a line starting with '#'.
std::optional<std::vector<std::string>> completeDirective( const std::string &line, size_t column );

/// @brief Header names that complete the one typed in an #include line.
struct IncludeCompletion {
    /// Columns of the header name, after '<' or '"' up to the closing one or the line end: the
    /// part a picked name replaces.
    size_t start = 0;
    size_t end = 0;
    /// Header paths, a folder with a trailing '/', shortest first.
    std::vector<std::string> names;
};

/// @brief Completes the header name of a C #include line: the .h files and folders in the
///        compiler's include folders for <...>, in the source's folder for "...".
/// @param[in] line      Text of the line.
/// @param[in] column    Cursor column in @p line.
/// @param[in] directory Folder of the source file.
/// @return Nothing if the cursor is not in the header name of an #include line.
std::optional<IncludeCompletion> completeInclude( const std::string &line, size_t column,
                                                  const std::string &directory );
