#pragma once

#include <functional>
#include <string>

#include <app/commands.h>
#include <gui/icons.h>

class Session;

/// @brief Asks for a command to run after the frame is drawn.
///
/// Panels only read the Session while drawing; everything that changes it is requested here
/// and run by runRequests(), so no panel sees a half-changed Session.
/// @param[in] command Command to run.
void requestCommand( Command command );

/// @brief Asks for any change of the Session to run after the frame is drawn.
/// @param[in] action Change to make.
void requestAction( std::function<void( Session & )> action );

/// @brief Runs the requests of this frame, in order. While a dialog is open they are dropped,
///        as the user is answering it.
///
/// A command that needs a file first asks for it in a dialog and runs when the user chose one.
/// @param[in,out] session Session to change.
void runRequests( Session &session );

/// @brief Requests the commands whose keyboard shortcut was pressed this frame.
void readCommandShortcuts();

/// @return Its shortcut as shown in menus, such as "Ctrl+S"; empty if it has none.
const char *commandShortcut( Command command );

/// @brief Draws a menu item for a command and requests it when clicked.
/// @param[in] session Current session, for the label and whether it can run.
void commandMenuItem( const Session &session, Command command );

/// @brief Draws an icon button for a command, disabled while the command can't run, and
///        requests it when clicked.
/// @param[in] session Current session, for the tooltip and whether it can run.
void commandButton( const Session &session, const char *id, Command command, Icon icon );

/// @brief Runs an action at once, or after the user agrees to discard unsaved changes.
/// @param[in] session Current session, for the unsaved documents.
/// @param[in] action  Action to run.
void whenChangesDiscarded( const Session &session, std::function<void()> action );
