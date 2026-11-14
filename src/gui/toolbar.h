#pragma once

class Session;

/// @brief Draws the app badge and recent files dropdown.
void drawToolbarStart( const Session &session );

/// @brief Draws the open documents dropdown and Build, Run, Debug, Stop.
void drawToolbarEnd( const Session &session );
