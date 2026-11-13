#pragma once

class Session;

/// @brief Draws the Registers panel: flags, the conditions that hold, and x0-x30, sp and pc.
///        Values changed since the previous pause are highlighted.
/// @param[in] session Current session.
void drawRegistersPanel( const Session &session );
