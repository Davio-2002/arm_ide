#pragma once

#include <functional>
#include <string>

/// @brief Opens the Open File dialog.
/// @param[in] onChosen Called with the chosen absolute path.
void askForFileToOpen( const std::string &directory,
                       std::function<void( const std::string & )> onChosen );

/// @brief Opens the Save As dialog.
/// @param[in] suggestedPath Folder shown and name filled in.
/// @param[in] onChosen      Called with the absolute path to save to.
void askForFileToSave( const std::string &suggestedPath,
                       std::function<void( const std::string & )> onChosen );

/// @brief Asks whether to discard unsaved changes.
/// @param[in] onDiscard Called if the user discards.
void askToDiscardChanges( const std::string &documentName, std::function<void()> onDiscard );

bool isDialogOpen();

/// @brief Draws the open dialog and runs its callback on confirm; called once per frame.
void drawDialogs();
