#pragma once

#include <set>
#include <string>
#include <vector>

/// @brief Finds the words that complete a typed term, shortest first.
///
/// In a case-insensitive language (assembly) the term matches ignoring case, and a term typed
/// in upper case gets the language words in upper case.
/// @param[in] term          Typed start of a word.
/// @param[in] languageWords Mnemonics, directives, registers or keywords of the language;
///                          lower case when the language ignores case.
/// @param[in] documentWords Identifiers found in the document.
/// @param[in] caseSensitive True if the language tells upper and lower case apart (C).
/// @return Words longer than the term that start with it.
std::vector<std::string> completeWord( const std::string &term,
                                       const std::set<std::string> &languageWords,
                                       const std::vector<std::string> &documentWords,
                                       bool caseSensitive );
