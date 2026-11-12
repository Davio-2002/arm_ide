#pragma once

#include <functional>
#include <string>

void askForFileToOpen( const std::string &directory,
                       std::function<void( const std::string & )> onChosen );
void askForFileToSave( const std::string &suggestedPath,
                       std::function<void( const std::string & )> onChosen );
void askToDiscardChanges( const std::string &documentName, std::function<void()> onDiscard );
// Theme colours, applied while they are picked.
void openAccentColorDialog();
void openPanelColorDialog();
bool isDialogOpen();
void drawDialogs();
