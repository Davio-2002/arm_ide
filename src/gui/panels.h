#pragma once

#include <string>

class Session;

void setupEditor( Session &session );
void drawPanels( Session &session );

void setEditorText( const std::string &text );
std::string editorText();
void markEditorSaved();
bool isEditorModified();
int editorCursorLine();
