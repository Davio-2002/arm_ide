#pragma once

#include <TextEditor.h>

/// @brief Syntax definition of C for the editor: the editor's own C, plus the C standard
///        library names it lacks, for autocomplete and colour.
/// @return The language, built on first call.
const TextEditor::Language *cLanguage();
