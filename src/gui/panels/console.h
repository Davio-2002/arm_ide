#pragma once

class Session;
struct MenuActions;

/// @brief Draws the Console panel with three tabs: program output, build log and terminal.
///
/// After each build the panel switches to the build log if it failed and to the program
/// output if it succeeded.
/// @param[in]     session Current session.
/// @param[in,out] actions Receives the header buttons, program input and terminal input.
void drawConsolePanel( const Session &session, MenuActions &actions );
