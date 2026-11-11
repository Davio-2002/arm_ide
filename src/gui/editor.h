#pragma once

#include <string>

class Session;

void setupEditor( Session &session );
void syncEditorWithSession( Session &session );
void drawCodeEditor();
void jumpToSourceLine( int sourceLine );

void setEditorText( const std::string &text );
std::string editorText();
void markEditorSaved();
bool isEditorModified();
int editorCursorLine();
