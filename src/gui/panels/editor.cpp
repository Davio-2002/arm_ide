#include <TextEditor.h>
#include <imgui.h>

#include <algorithm>
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

/// @brief The text being edited for one document of the session.
struct EditorBuffer {
    int documentId = 0;
    /// Path the language was chosen for.
    std::string path;
    TextEditor editor;
    int shownLineMarksVersion = -1;
    bool shownLineMarksOfProgram = false;
    /// Edit versions last looked at for the header names popup and the header declarations.
    size_t headerNamesCheckedVersion = 0;
    size_t declarationsCheckedVersion = 0;
    /// Frame after which the header names popup opens; 0 when none is waiting.
    int headerNamesFrame = 0;
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

/// @brief Autocomplete callback: suggests the words of the document's language and its
///        identifiers that start with the typed term.
/// @param[in,out] state Typed term in, suggestions out.
void suggestCompletions( TextEditor::AutoCompleteState &state ) {
    std::vector<std::string> documentWords;
    drawingBuffer->editor.IterateIdentifiers(
        [&documentWords]( const std::string &word ) { documentWords.push_back( word ); } );
    if ( sourceLanguage( drawingBuffer->path ) != SourceLanguage::C ) {
        state.suggestions = completeAssembly( state.searchTerm, documentWords );
        return;
    }
    std::string line = drawingBuffer->editor.GetLineText( state.searchTermEnd.line );
    std::string folder = std::filesystem::path( drawingBuffer->path ).parent_path();
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
    for ( const Document &document : drawingSession->documents() ) {
        if ( document.id == drawingBuffer->documentId ) {
            state.suggestions = completeC( state.searchTerm, document.declarations, documentWords );
        }
    }
}

/// @brief Picks the language from the path, as the build does.
///
/// In assembly a '.' is part of a word (directives, b.cond) and starts autocomplete; in C a
/// '#' starts it, for the directive.
/// @param[in,out] buffer Buffer to configure.
void setLanguage( EditorBuffer &buffer ) {
    bool isC = sourceLanguage( buffer.path ) == SourceLanguage::C;
    buffer.editor.SetLanguage( isC ? cLanguage() : aarch64Language() );

    TextEditor::AutoCompleteConfig autoComplete;
    autoComplete.triggerCharacters = isC ? "#" : ".";
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
    buffer->headerNamesCheckedVersion = buffer->editor.GetUndoIndex();
    buffer->declarationsCheckedVersion = buffer->editor.GetUndoIndex();
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

/// @return True if the cursor's line is a preprocessor line, starting with '#'.
bool onPreprocessorLine( const EditorBuffer &buffer ) {
    std::string line = buffer.editor.GetLineText( buffer.editor.GetMainCursorPosition().line );
    size_t start = line.find_first_not_of( " \t" );
    return start != std::string::npos && line[start] == '#';
}

/// @brief Gives the session the text of an edited C buffer, to read its header declarations
///        again if its preprocessor lines changed. Waits while the cursor is on a preprocessor
///        line, so an #include is not compiled at every typed character.
void updateDeclarations( Session &session, EditorBuffer &buffer ) {
    size_t version = buffer.editor.GetUndoIndex();
    if ( version == buffer.declarationsCheckedVersion ||
         sourceLanguage( buffer.path ) != SourceLanguage::C || onPreprocessorLine( buffer ) ) {
        return;
    }
    buffer.declarationsCheckedVersion = version;
    session.updateDeclarations( buffer.documentId, buffer.editor.GetText() );
}

/// @brief Opens autocomplete when an edit leaves the cursor just after the '<' or '"' of a C
///        #include line, so the header names show as soon as one is typed.
///
/// The editor has no call to open it, so its trigger shortcut (Ctrl+Space) is queued as input.
/// That waits one frame: a popup still open closes only in the next one, and the editor
/// ignores the shortcut while it is open.
void openHeaderNames( EditorBuffer &buffer ) {
    if ( buffer.headerNamesFrame != 0 && ImGui::GetFrameCount() > buffer.headerNamesFrame ) {
        buffer.headerNamesFrame = 0;
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent( ImGuiMod_Ctrl, true );
        io.AddKeyEvent( ImGuiKey_Space, true );
        io.AddKeyEvent( ImGuiKey_Space, false );
        io.AddKeyEvent( ImGuiMod_Ctrl, false );
    }

    size_t version = buffer.editor.GetUndoIndex();
    if ( version == buffer.headerNamesCheckedVersion ||
         sourceLanguage( buffer.path ) != SourceLanguage::C ) {
        return;
    }
    buffer.headerNamesCheckedVersion = version;
    TextEditor::DocPos cursor = buffer.editor.GetMainCursorPosition();
    std::string line = buffer.editor.GetLineText( cursor.line );
    std::string folder = std::filesystem::path( buffer.path ).parent_path();
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
            buffer = addBuffer( document );
        }
        if ( buffer->path != document.path ) {
            buffer->path = document.path;
            setLanguage( *buffer );
        }
        session.setDocumentVersion( document.id, buffer->editor.GetUndoIndex() );
        updateDeclarations( session, *buffer );
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
        openHeaderNames( *drawingBuffer );
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
