#pragma once

#include <string>
#include <vector>

class Session;

void setupEditor( Session &session );
void syncEditorWithSession( Session &session );
void drawEditorPanel();

void openInEditor( const std::string &path, const std::string &text );
void jumpToSourceLine( int sourceLine );

// These work on the active tab.
std::string editorText();
void markEditorSaved( const std::string &path );
bool isEditorModified();
int editorCursorLine();
int editorCursorColumn();

std::vector<std::string> unsavedDocumentNames();
std::vector<std::string> openDocumentPaths();
