#pragma once

#include <string>

#include <gui/icons.h>

/// @brief Draws a flat icon button: transparent until hovered, dimmed while disabled.
/// @param[in] id      ImGui ID.
/// @param[in] icon    Icon shown.
/// @param[in] tooltip Text shown on hover.
/// @param[in] enabled False to draw the button disabled.
/// @return True if clicked.
bool iconButton( const char *id, Icon icon, const char *tooltip, bool enabled = true );

/// @brief Draws an icon button that stays highlighted while on. A click flips the state.
/// @param[in]     id      ImGui ID.
/// @param[in]     icon    Icon shown.
/// @param[in]     tooltip Text shown on hover.
/// @param[in,out] on      Toggle state.
void iconToggle( const char *id, Icon icon, const char *tooltip, bool &on );

/// @brief Draws a rounded accent outline around the last item, as on hovered toolbar items
///        and edited text fields.
/// @param[in] show False to draw nothing.
void outlineLastItem( bool show );

/// @brief Draws a short vertical line between groups in a header row.
void drawVerticalSeparator();

/// @brief A rounded tab for the header row of a panel.
struct PillTab {
    std::string label;
    Icon icon = Icon::AsmFile;
    bool hasIcon = false;
    bool selected = false;
    bool closable = false;
    bool unsaved = false;
};

/// @brief What a click on a PillTab hit.
enum class TabClick { None, Select, Close };

/// @brief Draws a pill tab.
/// @param[in] id  ImGui ID.
/// @param[in] tab Tab to draw.
/// @return Whether the tab or its × was clicked.
TabClick drawPillTab( const char *id, const PillTab &tab );
