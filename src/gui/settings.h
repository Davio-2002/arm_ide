#pragma once

class Session;

/// @brief Registers the IDE's settings handler and loads $XDG_CONFIG_HOME/arm_ide/imgui.ini:
///        dock layout, zoom, fonts, theme and recent files.
/// @param[in,out] session Session whose recent files are read and written.
void loadSettings( Session &session );
