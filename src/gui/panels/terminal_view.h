#pragma once

class Session;
struct MenuActions;

/// @brief Draws the shell's screen in the rest of the panel and sizes the terminal to fit.
///
/// It takes the keyboard after a click. The mouse wheel scrolls back, a drag selects text.
/// @param[in]     session Current session.
/// @param[in,out] actions Receives the terminal size, keys and pasted text.
void drawTerminalView( const Session &session, MenuActions &actions );

/// @brief Gives the keyboard to the terminal view.
void focusTerminalView();
