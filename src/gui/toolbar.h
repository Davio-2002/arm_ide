#pragma once

class Session;
struct MenuActions;

/// @brief Draws the left end of the toolbar: app badge and recent files dropdown.
/// @param[in]     session Current session.
/// @param[in,out] actions Receives the file chosen in the dropdown.
void drawToolbarStart( const Session &session, MenuActions &actions );

/// @brief Draws the right end of the toolbar: open files dropdown and the Build, Run, Debug
///        and Stop buttons.
/// @param[in]     session Current session.
/// @param[in,out] actions Receives the clicked buttons and the chosen file.
void drawToolbarEnd( const Session &session, MenuActions &actions );
