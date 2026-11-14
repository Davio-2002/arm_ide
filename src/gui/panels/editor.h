#pragma once

#include <app/commands.h>

class Session;

/// @brief Brings the text buffers in line with the session's documents: a buffer for each new
///        document, none for closed ones, the language of each path. Reports each buffer's
///        edit version to the session and applies read-only, line marks and the line to reveal.
///        Called once per frame, before drawing.
void syncEditorWithSession( Session &session );

/// @brief Draws the Editor panel: document tabs and the active document.
void drawEditorPanel( const Session &session );

/// @return Text, edit version and cursor line of the active document, for running a command.
CommandInput editorCommandInput( const Session &session );

/// @param[out] line    1-based cursor line in the active document.
/// @param[out] column  1-based cursor column.
void editorCursor( const Session &session, int &line, int &column );
