#pragma once

#include <TextEditor.h>

// The editor's own C, plus the C standard library names it lacks, for autocomplete and colour.
const TextEditor::Language *cLanguage();
