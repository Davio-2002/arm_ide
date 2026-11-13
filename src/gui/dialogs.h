#pragma once

#include <functional>
#include <string>

/// @brief Opens the Open File dialog.
/// @param[in] directory Directory shown first.
/// @param[in] onChosen  Called with the absolute path of the chosen file.
void askForFileToOpen( const std::string &directory,
                       std::function<void( const std::string & )> onChosen );

/// @brief Opens the Save As dialog.
/// @param[in] suggestedPath Directory shown first and the file name filled in.
/// @param[in] onChosen      Called with the absolute path to save to.
void askForFileToSave( const std::string &suggestedPath,
                       std::function<void( const std::string & )> onChosen );

/// @brief Asks whether to discard unsaved changes.
/// @param[in] documentName Name shown in the question.
/// @param[in] onDiscard    Called if the user chooses Discard.
void askToDiscardChanges( const std::string &documentName, std::function<void()> onDiscard );

/// @brief Opens the accent colour picker. The colour applies while it is picked.
void openAccentColorDialog();

/// @brief Opens the panel background colour picker. The colour applies while it is picked.
void openPanelColorDialog();

/// @return True while any dialog is open.
bool isDialogOpen();

/// @brief Draws the open dialog, if any, and runs its callback when the user confirms.
///        Called once per frame.
void drawDialogs();
