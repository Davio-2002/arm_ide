#include <TextEditor.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <set>

#include <app/session.h>
#include <gui/aarch64_language.h>
#include <gui/editor.h>
#include <gui/theme.h>
#include <gui/window.h>

static TextEditor editor;
static size_t savedUndoIndex = 0;
static int shownLineMarksVersion = -1;
static Session *editorSession = nullptr;

static void addLineMarker( const LineMark &mark ) {
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
    int line = static_cast<int>( decorator.line ) + 1;
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( "##breakpoint", ImVec2( decorator.width, decorator.height ) ) ) {
        session.toggleBreakpoint( line );
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float size = decorator.height;
    ImVec2 center( topLeft.x + size * 0.5f, topLeft.y + size * 0.5f );
    float radius = size * 0.3f;
    if ( session.breakpoints.contains( line ) ) {
        drawList->AddCircleFilled( center, radius, breakpointColor );
    } else if ( ImGui::IsItemHovered() ) {
        drawList->AddCircleFilled( center, radius, breakpointHoverColor );
    }

    if ( line == session.pausedLine ) {
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
    editor.IterateIdentifiers( [&words]( const std::string &word ) { words.insert( word ); } );

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

void setupEditor( Session &session ) {
    editorSession = &session;
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

void setEditorText( const std::string &text ) {
    editor.SetText( text );
    markEditorSaved();
}

std::string editorText() {
    return editor.GetText();
}

void markEditorSaved() {
    savedUndoIndex = editor.GetUndoIndex();
}

bool isEditorModified() {
    return editor.GetUndoIndex() != savedUndoIndex;
}

int editorCursorLine() {
    return static_cast<int>( editor.GetCurrentCursorPosition().line ) + 1;
}

void jumpToSourceLine( int sourceLine ) {
    size_t line = sourceLine - 1;
    editor.SetCursor( TextEditor::DocPos( line, 0 ) );
    editor.ScrollToLine( line );
    ImGui::SetWindowFocus( "Editor" );
}

void syncEditorWithSession( Session &session ) {
    editor.SetReadOnlyEnabled( session.isDebugging() );

    if ( shownLineMarksVersion != session.lineMarksVersion ) {
        shownLineMarksVersion = session.lineMarksVersion;
        editor.ClearMarkers();
        for ( const LineMark &mark : session.lineMarks ) {
            addLineMarker( mark );
        }
    }

    if ( session.lineToReveal > 0 ) {
        editor.SetCursor( TextEditor::DocPos( session.lineToReveal - 1, 0 ) );
        session.lineToReveal = 0;
    }
}

void drawCodeEditor() {
    pushCodeFont();
    editor.Render( "##code" );
    ImGui::PopFont();
}
