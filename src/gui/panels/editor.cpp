#include <TextEditor.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
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

/// @brief The edited text of one session document.
struct EditorBuffer {
    int documentId = 0;
    std::string path;
    TextEditor editor;
    size_t shownVersion = 0;
    size_t reportedUndoIndex = 0;
    int shownLineMarksVersion = -1;
    bool shownLineMarksOfProgram = false;
    size_t headerNamesCheckedVersion = 0;
    size_t declarationsCheckedVersion = 0;
    int headerNamesFrame = 0;
};

std::vector<std::unique_ptr<EditorBuffer>> buffers;

/// @return The buffer, or nullptr before the next sync.
EditorBuffer *findBuffer( int documentId ) {
    for ( const std::unique_ptr<EditorBuffer> &buffer : buffers ) {
        if ( buffer->documentId == documentId ) {
            return buffer.get();
        }
    }
    return nullptr;
}

/// @return True if the buffer shows the built file, which owns build errors and the paused line.
bool showsProgram( const EditorBuffer &buffer, const Session &session ) {
    return !buffer.path.empty() && buffer.path == session.programFilePath;
}

/// @brief Colours a marked line by kind; its message becomes the tooltip.
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

void drawPausedArrow( ImDrawList *drawList, ImVec2 center, float size ) {
    float const half = size * 0.26f;
    float const left = center.x - size * 0.3f;
    float const neck = center.x + size * 0.02f;
    float const tip = center.x + size * 0.34f;
    float const body = half * 0.55f;
    std::array const points = {
        ImVec2( left, center.y - body ), ImVec2( neck, center.y - body ),
        ImVec2( neck, center.y - half ), ImVec2( tip, center.y ),
        ImVec2( neck, center.y + half ), ImVec2( neck, center.y + body ),
        ImVec2( left, center.y + body ),
    };
    drawList->PathClear();
    for ( ImVec2 const point : points ) {
        drawList->PathLineTo( point );
    }
    drawList->PathStroke( currentScheme().background, ImDrawFlags_Closed, 2.0f );
    drawList->AddConcavePolyFilled( points.data(), points.size(), accentColor() );
}

/// @brief Line decorator: breakpoint dot and paused arrow; a click toggles the breakpoint.
void drawLineDecoration( const Session &session, const EditorBuffer &buffer,
                         TextEditor::Decorator &decorator ) {
    int const line = static_cast<int>( decorator.line ) + 1;
    ImVec2 const topLeft = ImGui::GetCursorScreenPos();
    if ( ImGui::InvisibleButton( "##breakpoint", ImVec2( decorator.width, decorator.height ) ) ) {
        std::string const path = buffer.path;
        requestAction(
            [path, line]( Session &session ) { session.toggleBreakpoint( path, line ); } );
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float const size = decorator.height;
    ImVec2 const center( topLeft.x + size * 0.5f, topLeft.y + size * 0.5f );
    float const radius = size * 0.3f;
    if ( session.hasBreakpoint( buffer.path, line ) ) {
        drawList->AddCircleFilled( center, radius, currentScheme().breakpoint );
    } else if ( ImGui::IsItemHovered() ) {
        drawList->AddCircleFilled( center, radius, withAlpha( currentScheme().breakpoint, 90 ) );
    }
    if ( line == session.paused.line && showsProgram( buffer, session ) ) {
        drawPausedArrow( drawList, center, size );
    }
}

/// @brief Autocomplete callback: language words and document identifiers starting with the term.
void suggestCompletions( const Session &session, const EditorBuffer &buffer,
                         TextEditor::AutoCompleteState &state ) {
    std::vector<std::string> documentWords;
    buffer.editor.IterateIdentifiers(
        [&documentWords]( const std::string &word ) { documentWords.push_back( word ); } );
    if ( sourceLanguage( buffer.path ) != SourceLanguage::C ) {
        state.suggestions = completeAssembly( state.searchTerm, documentWords );
        return;
    }
    std::string const line = buffer.editor.GetLineText( state.searchTermEnd.line );
    std::string const folder = std::filesystem::path( buffer.path ).parent_path();
    if ( std::optional<IncludeCompletion> include =
             completeInclude( line, state.searchTermEnd.index, folder ) ) {
        state.suggestions = include->names;
        state.replacementStart.index = include->start;
        state.replacementEnd.index = include->end;
        return;
    }
    if ( std::optional<std::vector<std::string>> directives =
             completeDirective( line, state.searchTermEnd.index ) ) {
        state.suggestions = *directives;
        return;
    }
    if ( state.searchTerm.empty() ) {
        state.suggestions.clear();
        return;
    }
    for ( const Document &document : session.documents() ) {
        if ( document.id == buffer.documentId ) {
            state.suggestions = completeC( state.searchTerm, document.declarations, documentWords );
        }
    }
}

/// @brief Picks the language by path. Assembly: '.' is a word character and triggers
///        autocomplete; C: '#' triggers it.
/// @param[in] session Read by the completion callback; must outlive the buffer.
void setLanguage( EditorBuffer &buffer, const Session &session ) {
    bool const isC = sourceLanguage( buffer.path ) == SourceLanguage::C;
    buffer.editor.SetLanguage( isC ? cLanguage() : aarch64Language() );

    TextEditor::AutoCompleteConfig autoComplete;
    autoComplete.triggerCharacters = isC ? "#" : ".";
    autoComplete.wordCharacters = isC ? "" : ".";
    autoComplete.callback = [&session, &buffer]( TextEditor::AutoCompleteState &state ) {
        suggestCompletions( session, buffer, state );
    };
    buffer.editor.SetAutoCompleteConfig( &autoComplete );
}

/// @param[in] session Read by the editor's callbacks; must outlive the buffer.
EditorBuffer *addBuffer( const Document &document, const Session &session ) {
    auto buffer = std::make_unique<EditorBuffer>();
    buffer->documentId = document.id;
    buffer->path = document.path;
    buffer->editor.SetTabSize( 5 );
    buffer->editor.SetInsertSpacesOnTabs( true );
    buffer->editor.SetShowWhitespacesEnabled( false );
    buffer->editor.SetLineDecorator(
        2, [&session, &buffer = *buffer]( TextEditor::Decorator &decorator ) {
            drawLineDecoration( session, buffer, decorator );
        } );
    setLanguage( *buffer, session );
    buffer->editor.SetText( document.text );
    buffer->shownVersion = document.version;
    buffer->reportedUndoIndex = buffer->editor.GetUndoIndex();
    buffer->headerNamesCheckedVersion = buffer->editor.GetUndoIndex();
    buffer->declarationsCheckedVersion = document.version;
    buffers.push_back( std::move( buffer ) );
    return buffers.back().get();
}

/// @brief Rebuilds markers when the line marks changed; only the built file shows them.
void showLineMarks( EditorBuffer &buffer, const Session &session ) {
    bool const marksVisible = showsProgram( buffer, session );
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

/// @brief Tab row; selecting and closing are requested for after the frame.
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
        TabClick const click = drawPillTab( "##tab", tab );
        if ( !document.path.empty() ) {
            ImGui::SetItemTooltip( "%s", document.path.c_str() );
        }
        ImGui::PopID();
        ImGui::SameLine( 0.0f, 2.0f );

        int const id = document.id;
        if ( click == TabClick::Select ) {
            requestAction( [id]( Session &session ) { session.activateDocument( id ); } );
        } else if ( click == TabClick::Close ) {
            requestAction( [id]( Session &session ) { closeAsking( session, id ); } );
        }
    }
    ImGui::NewLine();
}

/// @return True if the cursor line starts with '#'.
bool onPreprocessorLine( const EditorBuffer &buffer ) {
    std::string line = buffer.editor.GetLineText( buffer.editor.GetMainCursorPosition().line );
    size_t const start = line.find_first_not_of( " \t" );
    return start != std::string::npos && line[start] == '#';
}

/// @brief Asks the session to reread an edited C document's header declarations, once the
///        cursor is off a '#' line, so #include isn't compiled at every keystroke.
void updateDeclarations( Session &session, EditorBuffer &buffer, const Document &document ) {
    if ( document.version == buffer.declarationsCheckedVersion ||
         sourceLanguage( buffer.path ) != SourceLanguage::C || onPreprocessorLine( buffer ) ) {
        return;
    }
    buffer.declarationsCheckedVersion = document.version;
    session.updateDeclarations( buffer.documentId );
}

/// @brief Shows the document's text when the session changed it; the buffer's own edits
///        were reported first, so a newer version here comes from the session.
void showDocumentText( EditorBuffer &buffer, const Document &document ) {
    if ( buffer.shownVersion == document.version ) {
        return;
    }
    TextEditor::DocPos const cursor = buffer.editor.GetMainCursorPosition();
    buffer.editor.SetText( document.text );
    buffer.editor.SetCursor( cursor );
    buffer.shownVersion = document.version;
    buffer.reportedUndoIndex = buffer.editor.GetUndoIndex();
}

/// @brief Opens autocomplete after an edit leaves the cursor just after an #include's '<' or
///        '"'. The editor has no call for it, so Ctrl+Space is queued as input one frame later:
///        an open popup closes only next frame and the shortcut is ignored meanwhile.
void openHeaderNames( EditorBuffer &buffer ) {
    if ( buffer.headerNamesFrame != 0 && ImGui::GetFrameCount() > buffer.headerNamesFrame ) {
        buffer.headerNamesFrame = 0;
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent( ImGuiMod_Ctrl, true );
        io.AddKeyEvent( ImGuiKey_Space, true );
        io.AddKeyEvent( ImGuiKey_Space, false );
        io.AddKeyEvent( ImGuiMod_Ctrl, false );
    }

    size_t const version = buffer.editor.GetUndoIndex();
    if ( version == buffer.headerNamesCheckedVersion ||
         sourceLanguage( buffer.path ) != SourceLanguage::C ) {
        return;
    }
    buffer.headerNamesCheckedVersion = version;
    TextEditor::DocPos const cursor = buffer.editor.GetMainCursorPosition();
    std::string const line = buffer.editor.GetLineText( cursor.line );
    std::string const folder = std::filesystem::path( buffer.path ).parent_path();
    std::optional<IncludeCompletion> include = completeInclude( line, cursor.index, folder );
    if ( include && include->start == cursor.index ) {
        buffer.headerNamesFrame = ImGui::GetFrameCount();
    }
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
            buffer = addBuffer( document, session );
        }
        if ( buffer->path != document.path ) {
            buffer->path = document.path;
            setLanguage( *buffer, session );
        }
        showDocumentText( *buffer, document );
        updateDeclarations( session, *buffer, document );
        buffer->editor.SetReadOnlyEnabled( session.isDebugging() );
        showLineMarks( *buffer, session );
    }

    int const line = session.takeLineToReveal();
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
    if ( EditorBuffer *buffer = findBuffer( session.activeDocument().id ) ) {
        pushCodeFont();
        ImGui::PushID( buffer );
        buffer->editor.SetPalette( editorPalette() );
        buffer->editor.Render( "##code" );
        openHeaderNames( *buffer );
        ImGui::PopID();
        ImGui::PopFont();
    }
    ImGui::End();
}

void reportEditorEdits( Session &session ) {
    for ( const std::unique_ptr<EditorBuffer> &buffer : buffers ) {
        size_t const undoIndex = buffer->editor.GetUndoIndex();
        if ( undoIndex == buffer->reportedUndoIndex ) {
            continue;
        }
        buffer->reportedUndoIndex = undoIndex;
        session.editDocument( buffer->documentId, buffer->editor.GetText() );
        for ( const Document &document : session.documents() ) {
            if ( document.id == buffer->documentId ) {
                buffer->shownVersion = document.version;
            }
        }
    }
}

CommandInput editorCommandInput( const Session &session ) {
    CommandInput input;
    if ( EditorBuffer const *buffer = findBuffer( session.activeDocument().id ) ) {
        input.line = static_cast<int>( buffer->editor.GetCurrentCursorPosition().line ) + 1;
    }
    return input;
}

void editorCursor( const Session &session, int &line, int &column ) {
    line = 1;
    column = 1;
    if ( EditorBuffer const *buffer = findBuffer( session.activeDocument().id ) ) {
        TextEditor::DocPos const position = buffer->editor.GetCurrentCursorPosition();
        line = static_cast<int>( position.line ) + 1;
        column = static_cast<int>( position.index ) + 1;
    }
}
