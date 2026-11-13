#include <TextEditor.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <language/aarch64_language.h>
#include <language/c_language.h>
#include <memory>
#include <set>
#include <vector>

#include <app/session.h>
#include <gui/dialogs.h>
#include <gui/panels/editor.h>
#include <gui/panels/panels.h>
#include <gui/theme.h>
#include <gui/widgets.h>
#include <gui/window.h>

/// @brief One editor tab.
struct Document {
    std::string path;
    TextEditor editor;
    size_t savedUndoIndex = 0;
    int shownLineMarksVersion = -1;
    bool shownLineMarksOfProgram = false;
};

static std::vector<std::unique_ptr<Document>> documents;
static Document *activeDocument = nullptr;
static Document *drawingDocument = nullptr;
static Session *editorSession = nullptr;

/// @param[in] document Tab to check.
/// @return True if the tab changed since it was opened or saved.
static bool isModified( const Document &document ) {
    return document.editor.GetUndoIndex() != document.savedUndoIndex;
}

/// @param[in] document Tab.
/// @return File name of the tab, or "untitled".
static std::string documentName( const Document &document ) {
    if ( document.path.empty() ) {
        return "untitled";
    }
    return std::filesystem::path( document.path ).filename();
}

/// @brief Tells if a tab shows the file last built, which owns the build errors and the
///        paused line.
/// @param[in] document Tab to check.
/// @return True if the tab shows the built file.
static bool showsProgram( const Document &document ) {
    return !document.path.empty() && document.path == editorSession->programFilePath;
}

/// @param[in] path Absolute file path.
/// @return The tab showing the file, or nullptr.
static Document *findDocument( const std::string &path ) {
    for ( const std::unique_ptr<Document> &document : documents ) {
        if ( document->path == path ) {
            return document.get();
        }
    }
    return nullptr;
}

/// @brief Colours a marked line and its number by the mark kind.
/// @param[in,out] editor Editor to mark.
/// @param[in]     mark   Line mark; its message becomes the tooltip.
static void addLineMarker( TextEditor &editor, const LineMark &mark ) {
    ImU32 numberColor = errorColor;
    ImU32 lineColor = errorLineColor;
    if ( mark.kind == LineMarkKind::Warning ) {
        numberColor = warningColor;
        lineColor = warningLineColor;
    } else if ( mark.kind == LineMarkKind::Paused ) {
        numberColor = pausedLineColor();
        lineColor = pausedLineColor();
    }
    editor.AddMarker( mark.line - 1, numberColor, lineColor, mark.message, mark.message );
}

/// @brief Line decorator callback: draws the breakpoint dot and the execution arrow left of
///        a line. A click toggles the breakpoint.
/// @param[in] decorator Line and size of the decoration area.
static void drawLineDecoration( TextEditor::Decorator &decorator ) {
    Session &session = *editorSession;
    const Document &document = *drawingDocument;
    int line = static_cast<int>( decorator.line ) + 1;
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( "##breakpoint", ImVec2( decorator.width, decorator.height ) ) ) {
        session.toggleBreakpoint( document.path, line );
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float size = decorator.height;
    ImVec2 center( topLeft.x + size * 0.5f, topLeft.y + size * 0.5f );
    float radius = size * 0.3f;
    if ( session.hasBreakpoint( document.path, line ) ) {
        drawList->AddCircleFilled( center, radius, breakpointColor );
    } else if ( ImGui::IsItemHovered() ) {
        drawList->AddCircleFilled( center, radius, breakpointHoverColor );
    }

    if ( line == session.pausedLine && showsProgram( document ) ) {
        float half = size * 0.26f;
        float left = center.x - size * 0.3f;
        float neck = center.x + size * 0.02f;
        float tip = center.x + size * 0.34f;
        float body = half * 0.55f;
        ImVec2 points[] = {
            ImVec2( left, center.y - body ), ImVec2( neck, center.y - body ),
            ImVec2( neck, center.y - half ), ImVec2( tip, center.y ),
            ImVec2( neck, center.y + half ), ImVec2( neck, center.y + body ),
            ImVec2( left, center.y + body ),
        };
        drawList->PathClear();
        for ( ImVec2 point : points ) {
            drawList->PathLineTo( point );
        }
        drawList->PathStroke( islandColor(), ImDrawFlags_Closed, 2.0f );
        drawList->AddConcavePolyFilled( points, IM_ARRAYSIZE( points ), accentColor() );
    }
}

/// @param[in] word   Word to check.
/// @param[in] prefix Prefix to look for.
/// @return True if word starts with prefix, ignoring case.
static bool startsWithIgnoringCase( const std::string &word, const std::string &prefix ) {
    return word.size() >= prefix.size() &&
           std::equal( prefix.begin(), prefix.end(), word.begin(), []( char a, char b ) {
               return std::tolower( static_cast<unsigned char>( a ) ) ==
                      std::tolower( static_cast<unsigned char>( b ) );
           } );
}

/// @param[in] word    Word to convert.
/// @param[in] convert std::toupper or std::tolower.
/// @return The word with each character converted.
static std::string withCase( std::string word, int ( *convert )( int ) ) {
    for ( char &c : word ) {
        c = static_cast<char>( convert( static_cast<unsigned char>( c ) ) );
    }
    return word;
}

/// @brief Autocomplete callback: suggests language words and identifiers of the document
///        that start with the typed term, shortest first.
/// @param[in,out] state Typed term in, suggestions out.
static void suggestCompletions( TextEditor::AutoCompleteState &state ) {
    const TextEditor::Language &language = *drawingDocument->editor.GetLanguage();
    const std::string &term = state.searchTerm;
    bool ignoresCase = !language.caseSensitive;
    bool upper = ignoresCase && term == withCase( term, std::toupper ) &&
                 term != withCase( term, std::tolower );
    std::set<std::string> languageWords;
    languageWords.insert( language.keywords.begin(), language.keywords.end() );
    languageWords.insert( language.declarations.begin(), language.declarations.end() );
    languageWords.insert( language.identifiers.begin(), language.identifiers.end() );

    std::set<std::string> words;
    for ( const std::string &word : languageWords ) {
        words.insert( upper ? withCase( word, std::toupper ) : word );
    }
    drawingDocument->editor.IterateIdentifiers( [&]( const std::string &word ) {
        if ( !languageWords.contains( ignoresCase ? withCase( word, std::tolower ) : word ) ) {
            words.insert( word );
        }
    } );

    state.suggestions.clear();
    for ( const std::string &word : words ) {
        if ( word.size() > state.searchTerm.size() &&
             startsWithIgnoringCase( word, state.searchTerm ) ) {
            state.suggestions.push_back( word );
        }
    }
    std::ranges::stable_sort( state.suggestions, []( const std::string &a, const std::string &b ) {
        return a.size() < b.size();
    } );
}

/// @brief Picks the language from the file extension: C for .c and .h files, AArch64
///        assembly for everything else.
///
/// In assembly a '.' is part of a word (directives, b.cond) and starts autocomplete.
/// @param[in,out] document Tab to configure.
static void setLanguage( Document &document ) {
    std::string extension = std::filesystem::path( document.path ).extension();
    bool isC = extension == ".c" || extension == ".h";
    document.editor.SetLanguage( isC ? cLanguage() : aarch64Language() );

    TextEditor::AutoCompleteConfig autoComplete;
    autoComplete.triggerCharacters = isC ? "" : ".";
    autoComplete.wordCharacters = isC ? "" : ".";
    autoComplete.callback = suggestCompletions;
    document.editor.SetAutoCompleteConfig( &autoComplete );
}

/// @brief Sets tab size, whitespace display and the breakpoint decorator.
/// @param[in,out] editor Editor to configure.
static void configureEditor( TextEditor &editor ) {
    editor.SetTabSize( 5 );
    editor.SetInsertSpacesOnTabs( true );
    editor.SetShowWhitespacesEnabled( false );
    editor.SetLineDecorator( 2, drawLineDecoration );
}

/// @brief Adds a tab at the end, marked as saved.
/// @param[in] path Absolute file path; empty for untitled.
/// @param[in] text Initial text.
/// @return The new tab.
static Document *addDocument( const std::string &path, const std::string &text ) {
    auto document = std::make_unique<Document>();
    document->path = path;
    configureEditor( document->editor );
    setLanguage( *document );
    document->editor.SetText( text );
    document->savedUndoIndex = document->editor.GetUndoIndex();
    documents.push_back( std::move( document ) );
    return documents.back().get();
}

/// @brief Makes a tab active. The active tab is the file Session builds, runs and saves.
/// @param[in] document Tab to activate.
static void makeActive( Document *document ) {
    activeDocument = document;
    editorSession->filePath = document->path;
}

/// @brief Closes a tab without asking. The last tab is replaced by an empty untitled one.
/// @param[in] document Tab to close.
static void closeDocument( Document *document ) {
    auto position = std::ranges::find( documents, document, &std::unique_ptr<Document>::get );
    size_t index = position - documents.begin();
    documents.erase( position );
    if ( documents.empty() ) {
        addDocument( "", "" );
    }
    if ( activeDocument == document ) {
        makeActive( documents[std::min( index, documents.size() - 1 )].get() );
    }
}

/// @brief Closes a tab, asking first if it has unsaved changes.
/// @param[in] document Tab to close.
static void askToCloseDocument( Document *document ) {
    if ( !isModified( *document ) ) {
        closeDocument( document );
        return;
    }
    askToDiscardChanges( documentName( *document ), [document] { closeDocument( document ); } );
}

void setupEditor( Session &session ) {
    editorSession = &session;
    makeActive( addDocument( "", "" ) );
}

void openInEditor( const std::string &path, const std::string &text ) {
    if ( Document *open = findDocument( path ) ) {
        makeActive( open );
        return;
    }

    bool replacesEmptyUntitled = activeDocument->path.empty() && !isModified( *activeDocument );
    Document *untitled = activeDocument;
    makeActive( addDocument( path, text ) );
    if ( replacesEmptyUntitled ) {
        closeDocument( untitled );
    }
}

void openNewDocument() {
    makeActive( addDocument( "", "" ) );
}

std::string editorText() {
    return activeDocument->editor.GetText();
}

void markEditorSaved( const std::string &path ) {
    activeDocument->path = path;
    setLanguage( *activeDocument );
    activeDocument->savedUndoIndex = activeDocument->editor.GetUndoIndex();
    makeActive( activeDocument );
}

bool isEditorModified() {
    return isModified( *activeDocument );
}

std::vector<std::string> unsavedDocumentNames() {
    std::vector<std::string> names;
    for ( const std::unique_ptr<Document> &document : documents ) {
        if ( isModified( *document ) ) {
            names.push_back( documentName( *document ) );
        }
    }
    return names;
}

std::vector<std::string> openDocumentPaths() {
    std::vector<std::string> paths;
    for ( const std::unique_ptr<Document> &document : documents ) {
        if ( !document->path.empty() ) {
            paths.push_back( document->path );
        }
    }
    return paths;
}

int editorCursorLine() {
    return static_cast<int>( activeDocument->editor.GetCurrentCursorPosition().line ) + 1;
}

int editorCursorColumn() {
    return static_cast<int>( activeDocument->editor.GetCurrentCursorPosition().index ) + 1;
}

void jumpToSourceLine( int sourceLine ) {
    auto *program = findDocument( editorSession->programFilePath );
    if ( !program ) {
        return;
    }
    makeActive( program );
    size_t line = sourceLine - 1;
    program->editor.SetCursor( TextEditor::DocPos( line, 0 ) );
    program->editor.ScrollToLine( line );
    ImGui::SetWindowFocus( "Editor" );
}

/// @brief Updates a tab's markers when the session's line marks changed. Only the built file
///        shows them.
/// @param[in,out] document Tab to update.
/// @param[in]     session  Current session.
static void showLineMarks( Document &document, const Session &session ) {
    bool marksVisible = showsProgram( document );
    if ( document.shownLineMarksVersion == session.lineMarksVersion &&
         document.shownLineMarksOfProgram == marksVisible ) {
        return;
    }
    document.shownLineMarksVersion = session.lineMarksVersion;
    document.shownLineMarksOfProgram = marksVisible;
    document.editor.ClearMarkers();
    if ( marksVisible ) {
        for ( const LineMark &mark : session.lineMarks ) {
            addLineMarker( document.editor, mark );
        }
    }
}

void syncEditorWithSession( Session &session ) {
    for ( const std::unique_ptr<Document> &document : documents ) {
        document->editor.SetReadOnlyEnabled( session.isDebugging() );
        showLineMarks( *document, session );
    }

    if ( session.lineToReveal > 0 ) {
        if ( Document *program = findDocument( session.programFilePath ) ) {
            makeActive( program );
            program->editor.SetCursor( TextEditor::DocPos( session.lineToReveal - 1, 0 ) );
        }
        session.lineToReveal = 0;
    }
}

/// @brief Draws the tab row. Selecting and closing apply after the row is drawn.
static void drawDocumentTabs() {
    Document *documentToShow = nullptr;
    Document *documentToClose = nullptr;
    for ( const std::unique_ptr<Document> &document : documents ) {
        PillTab tab;
        tab.label = documentName( *document );
        tab.icon = Icon::AsmFile;
        tab.hasIcon = true;
        tab.selected = document.get() == activeDocument;
        tab.closable = true;
        tab.unsaved = isModified( *document );

        ImGui::PushID( document.get() );
        TabClick click = drawPillTab( "##tab", tab );
        if ( !document->path.empty() ) {
            ImGui::SetItemTooltip( "%s", document->path.c_str() );
        }
        ImGui::PopID();
        ImGui::SameLine( 0.0f, 2.0f );

        if ( click == TabClick::Select ) {
            documentToShow = document.get();
        } else if ( click == TabClick::Close ) {
            documentToClose = document.get();
        }
    }
    ImGui::NewLine();

    if ( documentToShow ) {
        makeActive( documentToShow );
    }
    if ( documentToClose ) {
        askToCloseDocument( documentToClose );
    }
}

void drawEditorPanel() {
    beginPanel( "Editor" );
    drawDocumentTabs();
    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + editorTabsGap );
    drawingDocument = activeDocument;
    pushCodeFont();
    ImGui::PushID( activeDocument );
    activeDocument->editor.SetPalette( editorPalette() );
    activeDocument->editor.Render( "##code" );
    ImGui::PopID();
    ImGui::PopFont();
    ImGui::End();
}
