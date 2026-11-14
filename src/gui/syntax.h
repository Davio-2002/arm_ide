#pragma once

#include <TextEditor.h>

/// @brief Syntax definition of AArch64 GNU assembly for the editor: mnemonics, directives,
///        registers, comments and numbers. Case-insensitive.
/// @return The language, built on first call.
const TextEditor::Language *aarch64Language();

/// @brief Syntax definition of C for the editor: the editor's own C, plus the C standard
///        library names it lacks, for autocomplete and colour.
/// @return The language, built on first call.
const TextEditor::Language *cLanguage();
