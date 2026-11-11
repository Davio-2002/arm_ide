#pragma once

#include <string>

#include <gui/icons.h>

// A flat icon button: transparent until hovered, dimmed while disabled.
bool iconButton( const char *id, Icon icon, const char *tooltip, bool enabled = true );

// An icon button that stays highlighted while on; a click flips on.
void iconToggle( const char *id, Icon icon, const char *tooltip, bool &on );

// A short vertical line between groups in a header row.
void drawVerticalSeparator();

// A rounded tab for the header row of a panel.
struct PillTab {
    std::string label;
    Icon icon = Icon::AsmFile;
    bool hasIcon = false;
    bool selected = false;
    bool closable = false;
    bool unsaved = false; // shows a dot instead of the × until hovered
};

enum class TabClick { None, Select, Close };

TabClick drawPillTab( const char *id, const PillTab &tab );
