#pragma once

class Session;

/// @brief Draws the bottom status bar: file breadcrumb, program state, status text, cursor
///        position and language.
void drawStatusBar( const Session &session );
