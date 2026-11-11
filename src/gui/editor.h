#pragma once

#include <string>
#include <vector>

class Session;

void setupEditor( Session &session );
void syncEditorWithSession( Session &session );
void drawCodeEditor();

// Opens the file in a new tab, or brings its tab to the front if it is already open.
void openInEditor( const std::string &path, const std::string &text );
void jumpToSourceLine( int sourceLine );

// These work on the active tab.
std::string editorText();
void markEditorSaved( const std::string &path );
bool isEditorModified();
int editorCursorLine();

std::vector<std::string> unsavedDocumentNames();
