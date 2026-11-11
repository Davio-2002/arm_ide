#pragma once

#include <gui/icons.h>

class Session;
struct MenuActions;

// A flat icon button: transparent until hovered, dimmed while disabled.
bool iconButton( const char *id, Icon icon, const char *tooltip, bool enabled = true );

// Build, Run, Debug and Stop buttons at the right end of the main menu bar.
void drawToolbar( const Session &session, MenuActions &actions );
