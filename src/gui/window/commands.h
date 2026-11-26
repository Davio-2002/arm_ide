#pragma once

#include <functional>
#include <string>

#include <app/session/commands.h>
#include <gui/look/icons.h>

class Session;

/// @brief Queues a command to run after drawing, so no panel sees a half-changed Session.
void requestCommand( Command command );

/// @brief Queues any Session change to run after drawing.
void requestAction( std::function<void( Session & )> action );

/// @brief Passes the editor's edits to the session, then runs this frame's requests in order,
///        asking for a file first when a command needs one. Dropped while a dialog is open.
void runRequests( Session &session );

/// @brief Requests the commands whose shortcut was pressed.
void readCommandShortcuts();

/// @return Shortcut text, e.g. "Ctrl+S"; empty if none.
const char *commandShortcut( Command command );

/// @brief Menu item that requests the command when clicked.
void commandMenuItem( const Session &session, Command command );

/// @brief Icon button that requests the command; disabled while it can't run.
void commandButton( const Session &session, const char *id, Command command, Icon icon );

/// @brief Runs the action at once, or after the user agrees to discard unsaved changes.
void whenChangesDiscarded( const Session &session, std::function<void()> action );
