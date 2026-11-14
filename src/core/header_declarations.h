#pragma once

#include <string>
#include <vector>

/// @brief A function or macro that the headers of a C file declare.
struct Declaration {
    std::string name;
    /// Function type, for example "int (const char *, ...)", or the macro's parameters and value.
    std::string detail;
};

/// @return The preprocessor lines of a C text with their continuation lines: all that
///         headerDeclarations() reads of it.
std::string preprocessorLines( const std::string &text );

/// @brief Lists the functions and macros of the headers a C text includes, as the cross
///        compiler sees them. Only the preprocessor lines are compiled, so errors in the code
///        don't matter; a missing header ends the list at that header.
/// @param[in] text      C source text.
/// @param[in] directory Folder of the source, searched for #include "...".
/// @return Declarations, functions first; names starting with '_' are left out.
std::vector<Declaration> headerDeclarations( const std::string &text,
                                             const std::string &directory );

/// @return Folders the cross compiler searches for #include <...>, in its order. Asked once.
const std::vector<std::string> &systemIncludeDirectories();
