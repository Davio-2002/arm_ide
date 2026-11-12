#pragma once

class Session;
struct MenuActions;

// Program output, build log and the terminal, as tabs.
void drawConsolePanel( const Session &session, MenuActions &actions );
