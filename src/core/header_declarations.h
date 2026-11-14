#pragma once

#include <string>
#include <vector>

/// @brief A function or macro declared by the headers of a C file.
struct Declaration {
    std::string name;
    std::string detail;
};

/// @return The preprocessor lines of a C text, which is all headerDeclarations() reads.
std::string preprocessorLines( const std::string &text );

/// @brief Lists the functions and macros of the headers a C text includes. Only the
///        preprocessor lines are compiled; a missing header ends the list.
/// @param[in] text      C source.
/// @param[in] directory Source folder, searched for #include "...".
/// @return Functions first; names starting with '_' left out.
std::vector<Declaration> headerDeclarations( const std::string &text,
                                             const std::string &directory );

/// @return Folders searched for #include <...>, in order; asked once.
const std::vector<std::string> &systemIncludeDirectories();
