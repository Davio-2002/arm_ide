#pragma once

class Session;

/// @brief Draws the Memory panel: a hex and ASCII dump at an address typed by the user.
///        Memory is shown only while the debugger is paused.
/// @param[in,out] session Session that reads the memory.
void drawMemoryPanel( Session &session );
