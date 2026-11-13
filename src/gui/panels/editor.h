#pragma once

#include <string>
#include <vector>

class Session;

/// @brief Connects the editor to the session and opens one empty untitled tab.
/// @param[in,out] session Session the editor works with.
void setupEditor( Session &session );

/// @brief Applies the session state to the tabs: read-only while debugging, line marks, and
///        the line to reveal. Called once per frame.
/// @param[in,out] session Current session; its lineToReveal is consumed.
void syncEditorWithSession( Session &session );

/// @brief Draws the Editor panel: document tabs and the active document.
void drawEditorPanel();

/// @brief Shows a file in a tab, reusing its tab if already open.
///
/// An empty unmodified untitled tab is replaced.
/// @param[in] path Absolute path of the file.
/// @param[in] text File contents.
void openInEditor( const std::string &path, const std::string &text );

/// @brief Opens an untitled tab. Saving it asks for a name, whose extension (.s or .c) picks
///        the language.
void openNewDocument();

/// @brief Shows the built file at a line and focuses the editor.
/// @param[in] sourceLine 1-based line of the built file.
void jumpToSourceLine( int sourceLine );

/// @return Text of the active tab.
std::string editorText();

/// @brief Marks the active tab saved under a path; the language follows the extension.
/// @param[in] path Absolute path the tab was saved to.
void markEditorSaved( const std::string &path );

/// @return True if the active tab has unsaved changes.
bool isEditorModified();

/// @return 1-based cursor line in the active tab.
int editorCursorLine();

/// @return 1-based cursor column in the active tab.
int editorCursorColumn();

/// @return Names of all tabs with unsaved changes.
std::vector<std::string> unsavedDocumentNames();

/// @return Paths of all tabs that have a file.
std::vector<std::string> openDocumentPaths();
