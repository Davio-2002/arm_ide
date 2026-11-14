#pragma once

#include <string>

#include <gui/icons.h>

/// @brief Flat icon button, transparent until hovered.
/// @return True if clicked.
bool iconButton( const char *id, Icon icon, const char *tooltip, bool enabled = true );

/// @brief Icon button highlighted while on; a click flips it.
void iconToggle( const char *id, Icon icon, const char *tooltip, bool &on );

/// @brief Draws a rounded accent outline around the last item when `show`.
void outlineLastItem( bool show );

void drawVerticalSeparator();

/// @brief A rounded tab of a panel header.
struct PillTab {
    std::string label;
    Icon icon = Icon::AsmFile;
    bool hasIcon = false;
    bool selected = false;
    bool closable = false;
    bool unsaved = false;
};

enum class TabClick { None, Select, Close };

/// @return Whether the tab or its × was clicked.
TabClick drawPillTab( const char *id, const PillTab &tab );
