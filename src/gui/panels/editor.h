#pragma once

#include <app/commands.h>

class Session;

/// @brief Matches buffers to the session's documents, reports edit versions, and applies
///        read-only, line marks and the line to reveal. Called before drawing.
void syncEditorWithSession( Session &session );

/// @brief Draws the document tabs and the active document.
void drawEditorPanel( const Session &session );

/// @return Active document's text, edit version and cursor line.
CommandInput editorCommandInput( const Session &session );

/// @brief Gets the 1-based cursor line and column.
void editorCursor( const Session &session, int &line, int &column );
