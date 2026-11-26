#pragma once

#include <TextEditor.h>

/// @return AArch64 GNU assembly, case-insensitive; built on first call.
const TextEditor::Language *aarch64Language();

/// @return The editor's C plus the C library names it lacks; built on first call.
const TextEditor::Language *cLanguage();
