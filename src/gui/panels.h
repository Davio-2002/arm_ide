#pragma once

#include <string>

class Session;
struct MenuActions;

void setupEditor( Session &session );
void drawPanels( Session &session, MenuActions &actions );

void setEditorText( const std::string &text );
std::string editorText();
void markEditorSaved();
bool isEditorModified();
int editorCursorLine();
