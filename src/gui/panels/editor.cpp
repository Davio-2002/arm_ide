#include <TextEditor.h>
#include <imgui.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include <app/completion.h>
#include <app/session.h>
#include <gui/commands.h>
#include <gui/dialogs.h>
#include <gui/fonts.h>
#include <gui/panels/editor.h>
#include <gui/panels/panels.h>
#include <gui/syntax.h>
#include <gui/theme.h>
#include <gui/widgets.h>

namespace {

/// @brief The text being edited for one document of the session.
struct EditorBuffer {
    int documentId = 0;
    /// Path the language was chosen for.
    std::string path;
    TextEditor editor;
    int shownLineMarksVersion = -1;
    bool shownLineMarksOfProgram = false;
};

std::vector<std::unique_ptr<EditorBuffer>> buffers;

/// Session and buffer being drawn, for the editor's callbacks, which get no user data.
const Session *drawingSession = nullptr;
EditorBuffer *drawingBuffer = nullptr;

/// @param[in] documentId Document of the session.
/// @return Its buffer, or nullptr before the next sync.
EditorBuffer *findBuffer( int documentId ) {
    for ( const std::unique_ptr<EditorBuffer> &buffer : buffers ) {
        if ( buffer->documentId == documentId ) {
            return buffer.get();
        }
    }
    return nullptr;
}

/// @brief Tells if a buffer shows the file last built, which owns the build errors and the
///        paused line.
/// @param[in] buffer  Buffer to check.
/// @return True if the buffer shows the built file.
bool showsProgram( const EditorBuffer &buffer, const Session &session ) {
    return !buffer.path.empty() && buffer.path == session.programFilePath;
}

/// @brief Colours a marked line and its number by the mark kind.
/// @param[in,out] editor Editor to mark.
/// @param[in]     mark   Line mark; its message becomes the tooltip.
void addLineMarker( TextEditor &editor, const LineMark &mark ) {
    ImU32 numberColor = currentScheme().error;
    ImU32 lineColor = withAlpha( currentScheme().error, 40 );
    if ( mark.kind == LineMarkKind::Warning ) {
        numberColor = currentScheme().warning;
        lineColor = withAlpha( currentScheme().warning, 30 );
    } else if ( mark.kind == LineMarkKind::Paused ) {
        numberColor = pausedLineColor();
        lineColor = pausedLineColor();
    }
    editor.AddMarker( mark.line - 1, numberColor, lineColor, mark.message, mark.message );
}

/// @brief Draws the arrow of the line the program is paused on.
/// @param[in] center   Centre of the decoration area.
/// @param[in] size     Height of the decoration area.
void drawPausedArrow( ImDrawList *drawList, ImVec2 center, float size ) {
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
    drawList->PathStroke( currentScheme().background, ImDrawFlags_Closed, 2.0f );
    drawList->AddConcavePolyFilled( points, IM_ARRAYSIZE( points ), accentColor() );
}

/// @brief Line decorator callback: draws the breakpoint dot and the execution arrow left of
///        a line. A click toggles the breakpoint.
/// @param[in] decorator Line and size of the decoration area.
void drawLineDecoration( TextEditor::Decorator &decorator ) {
    const Session &session = *drawingSession;
    const EditorBuffer &buffer = *drawingBuffer;
    int line = static_cast<int>( decorator.line ) + 1;
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( "##breakpoint", ImVec2( decorator.width, decorator.height ) ) ) {
        std::string path = buffer.path;
        requestAction(
            [path, line]( Session &session ) { session.toggleBreakpoint( path, line ); } );
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float size = decorator.height;
    ImVec2 center( topLeft.x + size * 0.5f, topLeft.y + size * 0.5f );
    float radius = size * 0.3f;
    if ( session.hasBreakpoint( buffer.path, line ) ) {
        drawList->AddCircleFilled( center, radius, currentScheme().breakpoint );
    } else if ( ImGui::IsItemHovered() ) {
        drawList->AddCircleFilled( center, radius, withAlpha( currentScheme().breakpoint, 90 ) );
    }
    if ( line == session.paused.line && showsProgram( buffer, session ) ) {
        drawPausedArrow( drawList, center, size );
    }
}

/// @brief Autocomplete callback: suggests the language's words and the document's identifiers
///        that start with the typed term.
/// @param[in,out] state Typed term in, suggestions out.
void suggestCompletions( TextEditor::AutoCompleteState &state ) {
    const TextEditor::Language &language = *drawingBuffer->editor.GetLanguage();
    std::set<std::string> languageWords;
    languageWords.insert( language.keywords.begin(), language.keywords.end() );
    languageWords.insert( language.declarations.begin(), language.declarations.end() );
    languageWords.insert( language.identifiers.begin(), language.identifiers.end() );
    std::vector<std::string> documentWords;
    drawingBuffer->editor.IterateIdentifiers(
        [&documentWords]( const std::string &word ) { documentWords.push_back( word ); } );
    state.suggestions =
        completeWord( state.searchTerm, languageWords, documentWords, language.caseSensitive );
}

/// @brief Picks the language from the path, as the build does.
///
/// In assembly a '.' is part of a word (directives, b.cond) and starts autocomplete.
/// @param[in,out] buffer Buffer to configure.
void setLanguage( EditorBuffer &buffer ) {
    bool isC = sourceLanguage( buffer.path ) == SourceLanguage::C;
    buffer.editor.SetLanguage( isC ? cLanguage() : aarch64Language() );

    TextEditor::AutoCompleteConfig autoComplete;
    autoComplete.triggerCharacters = isC ? "" : ".";
    autoComplete.wordCharacters = isC ? "" : ".";
    autoComplete.callback = suggestCompletions;
    buffer.editor.SetAutoCompleteConfig( &autoComplete );
}

/// @brief Adds the buffer of a new document, with its text and language.
/// @param[in] document Document of the session.
/// @return The new buffer.
EditorBuffer *addBuffer( const Document &document ) {
    auto buffer = std::make_unique<EditorBuffer>();
    buffer->documentId = document.id;
    buffer->path = document.path;
    buffer->editor.SetTabSize( 5 );
    buffer->editor.SetInsertSpacesOnTabs( true );
    buffer->editor.SetShowWhitespacesEnabled( false );
    buffer->editor.SetLineDecorator( 2, drawLineDecoration );
    setLanguage( *buffer );
    buffer->editor.SetText( document.diskText );
    buffers.push_back( std::move( buffer ) );
    return buffers.back().get();
}

/// @brief Updates a buffer's markers when the session's line marks changed. Only the built
///        file shows them.
/// @param[in,out] buffer  Buffer to update.
void showLineMarks( EditorBuffer &buffer, const Session &session ) {
    bool marksVisible = showsProgram( buffer, session );
    if ( buffer.shownLineMarksVersion == session.lineMarksVersion &&
         buffer.shownLineMarksOfProgram == marksVisible ) {
        return;
    }
    buffer.shownLineMarksVersion = session.lineMarksVersion;
    buffer.shownLineMarksOfProgram = marksVisible;
    buffer.editor.ClearMarkers();
    if ( marksVisible ) {
        for ( const LineMark &mark : session.lineMarks ) {
            addLineMarker( buffer.editor, mark );
        }
    }
}

/// @brief Closes a document, asking first if it has unsaved changes.
/// @param[in]     documentId Document to close.
void closeAsking( Session &session, int documentId ) {
    for ( const Document &document : session.documents() ) {
        if ( document.id == documentId && isModified( document ) ) {
            askToDiscardChanges( documentName( document.path ),
                                 [&session, documentId] { session.closeDocument( documentId ); } );
            return;
        }
    }
    session.closeDocument( documentId );
}

/// @brief Draws the tab row. Selecting and closing are requested for after the frame.
void drawDocumentTabs( const Session &session ) {
    for ( const Document &document : session.documents() ) {
        PillTab tab;
        tab.label = documentName( document.path );
        tab.icon = Icon::AsmFile;
        tab.hasIcon = true;
        tab.selected = document.id == session.activeDocument().id;
        tab.closable = true;
        tab.unsaved = isModified( document );

        ImGui::PushID( document.id );
        TabClick click = drawPillTab( "##tab", tab );
        if ( !document.path.empty() ) {
            ImGui::SetItemTooltip( "%s", document.path.c_str() );
        }
        ImGui::PopID();
        ImGui::SameLine( 0.0f, 2.0f );

        int id = document.id;
        if ( click == TabClick::Select ) {
            requestAction( [id]( Session &session ) { session.activateDocument( id ); } );
        } else if ( click == TabClick::Close ) {
            requestAction( [id]( Session &session ) { closeAsking( session, id ); } );
        }
    }
    ImGui::NewLine();
}

} // anonymous namespace

void syncEditorWithSession( Session &session ) {
    std::erase_if( buffers, [&session]( const std::unique_ptr<EditorBuffer> &buffer ) {
        return std::ranges::none_of( session.documents(), [&buffer]( const Document &document ) {
            return document.id == buffer->documentId;
        } );
    } );
    for ( const Document &document : session.documents() ) {
        EditorBuffer *buffer = findBuffer( document.id );
        if ( !buffer ) {
            buffer = addBuffer( document );
        }
        if ( buffer->path != document.path ) {
            buffer->path = document.path;
            setLanguage( *buffer );
        }
        session.setDocumentVersion( document.id, buffer->editor.GetUndoIndex() );
        buffer->editor.SetReadOnlyEnabled( session.isDebugging() );
        showLineMarks( *buffer, session );
    }

    int line = session.takeLineToReveal();
    EditorBuffer *active = findBuffer( session.activeDocument().id );
    if ( line > 0 && active ) {
        active->editor.SetCursor( TextEditor::DocPos( line - 1, 0 ) );
        active->editor.ScrollToLine( line - 1 );
    }
}

void drawEditorPanel( const Session &session ) {
    beginPanel( "Editor" );
    drawDocumentTabs( session );
    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + editorTabsGap );
    drawingSession = &session;
    drawingBuffer = findBuffer( session.activeDocument().id );
    if ( drawingBuffer ) {
        pushCodeFont();
        ImGui::PushID( drawingBuffer );
        drawingBuffer->editor.SetPalette( editorPalette() );
        drawingBuffer->editor.Render( "##code" );
        ImGui::PopID();
        ImGui::PopFont();
    }
    ImGui::End();
}

CommandInput editorCommandInput( const Session &session ) {
    CommandInput input;
    if ( EditorBuffer *buffer = findBuffer( session.activeDocument().id ) ) {
        input.text = buffer->editor.GetText();
        input.version = buffer->editor.GetUndoIndex();
        input.line = static_cast<int>( buffer->editor.GetCurrentCursorPosition().line ) + 1;
    }
    return input;
}

void editorCursor( const Session &session, int &line, int &column ) {
    line = 1;
    column = 1;
    if ( EditorBuffer *buffer = findBuffer( session.activeDocument().id ) ) {
        TextEditor::DocPos position = buffer->editor.GetCurrentCursorPosition();
        line = static_cast<int>( position.line ) + 1;
        column = static_cast<int>( position.index ) + 1;
    }
}
