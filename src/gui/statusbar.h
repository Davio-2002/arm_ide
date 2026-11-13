#pragma once

class Session;

/// @brief Draws the bottom status bar: file breadcrumb, program state, status text, cursor
///        position and file format.
/// @param[in] session Current session.
void drawStatusBar( const Session &session );
