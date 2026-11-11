#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include <gui/dialogs.h>

enum class DialogKind { None, Open, Save, Discard };

struct DirectoryEntry {
    std::string name;
    bool isDirectory = false;
};

struct Dialog {
    DialogKind kind = DialogKind::None;
    bool needsOpening = false;
    std::filesystem::path directory;
    std::vector<DirectoryEntry> entries;
    char fileName[256] = "";
    std::string message;
    std::function<void( const std::string & )> onChosen;
    std::function<void()> onDiscard;
};

static Dialog dialog;

static std::filesystem::path cleanPath( const std::filesystem::path &path ) {
    std::error_code error;
    std::filesystem::path clean = std::filesystem::absolute( path, error ).lexically_normal();
    if ( clean.has_filename() || clean == clean.root_path() ) {
        return clean;
    }
    return clean.parent_path();
}

static bool isAssemblyFile( const std::filesystem::path &path ) {
    std::string extension = path.extension().string();
    return extension == ".s" || extension == ".S" || extension == ".asm";
}

static void enterDirectory( const std::filesystem::path &directory ) {
    dialog.directory = cleanPath( directory );
    dialog.entries.clear();

    std::error_code error;
    std::filesystem::directory_iterator end;
    for ( std::filesystem::directory_iterator it( dialog.directory, error ); !error && it != end;
          it.increment( error ) ) {
        std::string name = it->path().filename().string();
        bool isDirectory = it->is_directory( error );
        if ( !name.starts_with( '.' ) && ( isDirectory || isAssemblyFile( it->path() ) ) ) {
            dialog.entries.push_back( { name, isDirectory } );
        }
    }

    std::ranges::sort( dialog.entries, []( const DirectoryEntry &a, const DirectoryEntry &b ) {
        if ( a.isDirectory != b.isDirectory ) {
            return a.isDirectory;
        }
        return a.name < b.name;
    } );
}

static void setFileName( const std::string &name ) {
    name.copy( dialog.fileName, sizeof( dialog.fileName ) - 1 );
    dialog.fileName[std::min( name.size(), sizeof( dialog.fileName ) - 1 )] = '\0';
}

void askForFileToOpen( const std::string &directory,
                       std::function<void( const std::string & )> onChosen ) {
    dialog = Dialog();
    dialog.kind = DialogKind::Open;
    dialog.needsOpening = true;
    dialog.onChosen = std::move( onChosen );
    enterDirectory( directory );
}

void askForFileToSave( const std::string &suggestedPath,
                       std::function<void( const std::string & )> onChosen ) {
    std::filesystem::path path = cleanPath( suggestedPath );
    dialog = Dialog();
    dialog.kind = DialogKind::Save;
    dialog.needsOpening = true;
    dialog.onChosen = std::move( onChosen );
    enterDirectory( path.parent_path() );
    setFileName( path.filename().string() );
}

void askToDiscardChanges( const std::string &documentName, std::function<void()> onDiscard ) {
    dialog = Dialog();
    dialog.kind = DialogKind::Discard;
    dialog.needsOpening = true;
    dialog.message = "Discard unsaved changes in " + documentName + "?";
    dialog.onDiscard = std::move( onDiscard );
}

bool isDialogOpen() {
    return dialog.kind != DialogKind::None;
}

static const char *dialogTitle() {
    switch ( dialog.kind ) {
    case DialogKind::Open:
        return "Open File";
    case DialogKind::Save:
        return "Save As";
    case DialogKind::Discard:
    case DialogKind::None:
        break;
    }
    return "Unsaved Changes";
}

static bool cancelPressed() {
    return ImGui::Button( "Cancel" ) || ImGui::IsKeyPressed( ImGuiKey_Escape );
}

static bool drawDiscardDialog( std::function<void()> &finish ) {
    ImGui::TextUnformatted( dialog.message.c_str() );
    ImGui::Spacing();
    if ( ImGui::Button( "Discard" ) ) {
        finish = std::move( dialog.onDiscard );
        return true;
    }
    ImGui::SameLine();
    return cancelPressed();
}

static bool drawFileList( std::filesystem::path &nextDirectory ) {
    bool chosen = false;
    float height = ImGui::GetTextLineHeightWithSpacing() * 16;
    if ( !ImGui::BeginListBox( "##files", ImVec2( -FLT_MIN, height ) ) ) {
        return false;
    }
    if ( dialog.directory != dialog.directory.root_path() && ImGui::Selectable( "../" ) ) {
        nextDirectory = dialog.directory.parent_path();
    }
    for ( const DirectoryEntry &entry : dialog.entries ) {
        std::string label = entry.isDirectory ? entry.name + "/" : entry.name;
        bool selected = !entry.isDirectory && entry.name == dialog.fileName;
        if ( !ImGui::Selectable( label.c_str(), selected,
                                 ImGuiSelectableFlags_AllowDoubleClick ) ) {
            continue;
        }
        if ( entry.isDirectory ) {
            nextDirectory = dialog.directory / entry.name;
        } else {
            setFileName( entry.name );
            chosen = ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left );
        }
    }
    ImGui::EndListBox();
    return chosen;
}

static bool drawFileDialog( std::function<void()> &finish ) {
    bool isSave = dialog.kind == DialogKind::Save;
    ImGui::TextDisabled( "%s", dialog.directory.c_str() );

    std::filesystem::path nextDirectory;
    ImGui::PushItemWidth( ImGui::GetFontSize() * 32 );
    bool chosen = drawFileList( nextDirectory );
    if ( ImGui::IsWindowAppearing() ) {
        ImGui::SetKeyboardFocusHere();
    }
    chosen |= ImGui::InputText( "##name", dialog.fileName, sizeof( dialog.fileName ),
                                ImGuiInputTextFlags_EnterReturnsTrue );
    ImGui::PopItemWidth();

    std::filesystem::path target = cleanPath( dialog.directory / dialog.fileName );
    std::error_code error;
    bool exists = std::filesystem::exists( target, error );
    const char *acceptLabel = isSave ? ( exists ? "Replace" : "Save" ) : "Open";
    bool canAccept = dialog.fileName[0] != '\0' && ( isSave || exists );
    ImGui::BeginDisabled( !canAccept );
    chosen |= ImGui::Button( acceptLabel );
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool cancelled = cancelPressed();

    if ( chosen && canAccept && std::filesystem::is_directory( target, error ) ) {
        enterDirectory( target );
        setFileName( "" );
    } else if ( chosen && canAccept ) {
        finish = [onChosen = std::move( dialog.onChosen ), path = target.string()] {
            onChosen( path );
        };
        return true;
    } else if ( !nextDirectory.empty() ) {
        enterDirectory( nextDirectory );
        if ( !isSave ) {
            setFileName( "" );
        }
    }
    return cancelled;
}

void drawDialogs() {
    if ( !isDialogOpen() ) {
        return;
    }
    const char *title = dialogTitle();
    if ( dialog.needsOpening ) {
        ImGui::OpenPopup( title );
        dialog.needsOpening = false;
    }

    ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                             ImVec2( 0.5f, 0.5f ) );
    if ( !ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
        dialog = Dialog();
        return;
    }
    std::function<void()> finish;
    bool done =
        dialog.kind == DialogKind::Discard ? drawDiscardDialog( finish ) : drawFileDialog( finish );
    if ( done ) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();

    if ( done ) {
        dialog = Dialog();
        if ( finish ) {
            finish();
        }
    }
}
