#pragma once

class Session;
struct MenuActions;

// The shell's screen, filling the rest of the panel; it takes the keyboard after a click.
void drawTerminalView( const Session &session, MenuActions &actions );
void focusTerminalView();
