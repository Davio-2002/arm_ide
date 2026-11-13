#pragma once

#include <TextEditor.h>

/// @brief Syntax definition of AArch64 GNU assembly for the editor: mnemonics, directives,
///        registers, comments and numbers. Case-insensitive.
/// @return The language, built on first call.
const TextEditor::Language *aarch64Language();
