#include <TextEditor.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include <app/session.h>
#include <gui/aarch64_language.h>
#include <gui/dialogs.h>
#include <gui/editor.h>
#include <gui/theme.h>
#include <gui/window.h>

// One editor tab: a file (or an untitled text) with its own undo history and cursor.
struct Document {
    std::string path;
    TextEditor editor;
    size_t savedUndoIndex = 0;
    int shownLineMarksVersion = -1;
    bool shownLineMarksOfProgram = false;
};

static std::vector<std::unique_ptr<Document>> documents;
static Document *activeDocument = nullptr;
static Document *documentToSelect = nullptr;
static Document *drawingDocument = nullptr;
static Session *editorSession = nullptr;

static bool isModified( const Document &document ) {
    return document.editor.GetUndoIndex() != document.savedUndoIndex;
}

static std::string documentName( const Document &document ) {
    if ( document.path.empty() ) {
        return "untitled";
    }
    return std::filesystem::path( document.path ).filename();
}

// Build errors and the paused line belong to the file last built.
static bool showsProgram( const Document &document ) {
    return !document.path.empty() && document.path == editorSession->programFilePath;
}

static Document *findDocument( const std::string &path ) {
    for ( const std::unique_ptr<Document> &document : documents ) {
        if ( document->path == path ) {
            return document.get();
        }
    }
    return nullptr;
}

static void addLineMarker( TextEditor &editor, const LineMark &mark ) {
    ImU32 numberColor = errorColor;
    ImU32 lineColor = errorLineColor;
    if ( mark.kind == LineMarkKind::Warning ) {
        numberColor = warningColor;
        lineColor = warningLineColor;
    } else if ( mark.kind == LineMarkKind::Paused ) {
        numberColor = pausedColor;
        lineColor = pausedLineColor;
    }
    editor.AddMarker( mark.line - 1, numberColor, lineColor, mark.message, mark.message );
}

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
        float arrow = radius * 0.7f;
        ImVec2 top( center.x - arrow, center.y - arrow );
        ImVec2 bottom( center.x - arrow, center.y + arrow );
        ImVec2 tip( center.x + arrow, center.y );
        drawList->AddTriangleFilled( top, bottom, tip, pausedColor );
    }
}

static bool startsWithIgnoringCase( const std::string &word, const std::string &prefix ) {
    return word.size() >= prefix.size() &&
           std::equal( prefix.begin(), prefix.end(), word.begin(), []( char a, char b ) {
               return std::tolower( static_cast<unsigned char>( a ) ) ==
                      std::tolower( static_cast<unsigned char>( b ) );
           } );
}

static void suggestCompletions( TextEditor::AutoCompleteState &state ) {
    const TextEditor::Language &language = *aarch64Language();
    std::set<std::string> words;
    words.insert( language.keywords.begin(), language.keywords.end() );
    words.insert( language.declarations.begin(), language.declarations.end() );
    words.insert( language.identifiers.begin(), language.identifiers.end() );
    drawingDocument->editor.IterateIdentifiers(
        [&words]( const std::string &word ) { words.insert( word ); } );

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

static void configureEditor( TextEditor &editor ) {
    editor.SetPalette( editorPalette() );
    editor.SetLanguage( aarch64Language() );
    editor.SetTabSize( 8 );
    editor.SetShowWhitespacesEnabled( false );
    editor.SetLineDecorator( 2, drawLineDecoration );

    TextEditor::AutoCompleteConfig autoComplete;
    autoComplete.triggerCharacters = ".";
    autoComplete.wordCharacters = ".";
    autoComplete.callback = suggestCompletions;
    editor.SetAutoCompleteConfig( &autoComplete );
}

static Document *addDocument( const std::string &path, const std::string &text ) {
    auto document = std::make_unique<Document>();
    document->path = path;
    configureEditor( document->editor );
    document->editor.SetText( text );
    document->savedUndoIndex = document->editor.GetUndoIndex();
    documents.push_back( std::move( document ) );
    return documents.back().get();
}

// The active document is the one Session builds, runs and saves.
static void makeActive( Document *document ) {
    activeDocument = document;
    editorSession->filePath = document->path;
}

// Also brings its tab to the front on the next draw.
static void selectDocument( Document *document ) {
    makeActive( document );
    documentToSelect = document;
}

static void closeDocument( Document *document ) {
    auto position = std::ranges::find( documents, document, &std::unique_ptr<Document>::get );
    size_t index = position - documents.begin();
    documents.erase( position );
    if ( documentToSelect == document ) {
        documentToSelect = nullptr;
    }
    if ( documents.empty() ) {
        addDocument( "", "" );
    }
    if ( activeDocument == document ) {
        selectDocument( documents[std::min( index, documents.size() - 1 )].get() );
    }
}

static void askToCloseDocument( Document *document ) {
    if ( !isModified( *document ) ) {
        closeDocument( document );
        return;
    }
    askToDiscardChanges( documentName( *document ), [document] { closeDocument( document ); } );
}

void setupEditor( Session &session ) {
    editorSession = &session;
    selectDocument( addDocument( "", "" ) );
}

void openInEditor( const std::string &path, const std::string &text ) {
    if ( Document *open = findDocument( path ) ) {
        selectDocument( open );
        return;
    }

    bool replacesEmptyUntitled = activeDocument->path.empty() && !isModified( *activeDocument );
    Document *untitled = activeDocument;
    selectDocument( addDocument( path, text ) );
    if ( replacesEmptyUntitled ) {
        closeDocument( untitled );
    }
}

std::string editorText() {
    return activeDocument->editor.GetText();
}

void markEditorSaved( const std::string &path ) {
    activeDocument->path = path;
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

int editorCursorLine() {
    return static_cast<int>( activeDocument->editor.GetCurrentCursorPosition().line ) + 1;
}

void jumpToSourceLine( int sourceLine ) {
    auto* program = findDocument( editorSession->programFilePath );
    if ( !program ) {
        return;
    }
    selectDocument( program );
    size_t line = sourceLine - 1;
    program->editor.SetCursor( TextEditor::DocPos( line, 0 ) );
    program->editor.ScrollToLine( line );
    ImGui::SetWindowFocus( "Editor" );
}

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
            selectDocument( program );
            program->editor.SetCursor( TextEditor::DocPos( session.lineToReveal - 1, 0 ) );
        }
        session.lineToReveal = 0;
    }
}

static void drawDocumentTabs() {
    if ( !ImGui::BeginTabBar( "##documents", ImGuiTabBarFlags_Reorderable |
                                                 ImGuiTabBarFlags_FittingPolicyScroll ) ) {
        return;
    }

    Document *shownDocument = nullptr;
    Document *documentToClose = nullptr;
    for ( const std::unique_ptr<Document> &document : documents ) {
        ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
        if ( isModified( *document ) ) {
            flags |= ImGuiTabItemFlags_UnsavedDocument;
        }
        if ( document.get() == documentToSelect ) {
            flags |= ImGuiTabItemFlags_SetSelected;
        }

        bool open = true;
        ImGui::PushID( document.get() );
        if ( ImGui::BeginTabItem( documentName( *document ).c_str(), &open, flags ) ) {
            shownDocument = document.get();
            ImGui::EndTabItem();
        }
        if ( !document->path.empty() ) {
            ImGui::SetItemTooltip( "%s", document->path.c_str() );
        }
        ImGui::PopID();
        if ( !open ) {
            documentToClose = document.get();
        }
    }
    ImGui::EndTabBar();

    if ( documentToSelect ) {
        if ( shownDocument == documentToSelect ) {
            documentToSelect = nullptr;
        }
    } else if ( shownDocument && shownDocument != activeDocument ) {
        makeActive( shownDocument );
    }
    if ( documentToClose ) {
        askToCloseDocument( documentToClose );
    }
}

void drawCodeEditor() {
    drawDocumentTabs();
    drawingDocument = activeDocument;
    pushCodeFont();
    ImGui::PushID( activeDocument );
    activeDocument->editor.Render( "##code" );
    ImGui::PopID();
    ImGui::PopFont();
}
