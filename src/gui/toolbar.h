#pragma once

class Session;

/// @brief Draws the left end of the toolbar: app badge and recent files dropdown.
void drawToolbarStart( const Session &session );

/// @brief Draws the right end of the toolbar: open documents dropdown and the Build, Run, Debug
///        and Stop buttons.
void drawToolbarEnd( const Session &session );
