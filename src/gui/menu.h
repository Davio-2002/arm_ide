#pragma once

class Session;

/// @brief Draws the main menu bar with the toolbar, and reads the keyboard shortcuts.
///
/// Chosen commands are requested (gui/commands.h) and run after the frame; view actions such
/// as zoom apply at once.
void drawMenuBar( const Session &session );
