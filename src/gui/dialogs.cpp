#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <filesystem>
#include <span>
#include <vector>

#include <gui/dialogs.h>
#include <gui/theme.h>
#include <gui/widgets.h>

enum class DialogKind { None, Open, Save, Discard, AccentColor, PanelColor };

/// @brief One row of the file list.
struct DirectoryEntry {
    std::string name;
    bool isDirectory = false;
};

/// @brief State of the one dialog that can be open at a time.
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

/// @brief Makes a path absolute and normalized, without a trailing slash.
/// @param[in] path Any path.
/// @return Clean absolute path.
static std::filesystem::path cleanPath( const std::filesystem::path &path ) {
    std::error_code error;
    std::filesystem::path clean = std::filesystem::absolute( path, error ).lexically_normal();
    if ( clean.has_filename() || clean == clean.root_path() ) {
        return clean;
    }
    return clean.parent_path();
}

/// @param[in] path File path.
/// @return True for .s, .S and .asm files.
static bool isAssemblyFile( const std::filesystem::path &path ) {
    std::string extension = path.extension().string();
    return extension == ".s" || extension == ".S" || extension == ".asm";
}

/// @brief Shows a directory in the file list: subdirectories first, then assembly files.
///        Hidden entries are skipped.
/// @param[in] directory Directory to list.
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

/// @brief Fills the file name field, truncating a name that does not fit.
/// @param[in] name File name.
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

/// @brief Opens a dialog that needs no arguments.
/// @param[in] kind Dialog to open.
static void openDialog( DialogKind kind ) {
    dialog = Dialog();
    dialog.kind = kind;
    dialog.needsOpening = true;
}

void openAccentColorDialog() {
    openDialog( DialogKind::AccentColor );
}

void openPanelColorDialog() {
    openDialog( DialogKind::PanelColor );
}

/// @return True if the open dialog is a colour picker.
static bool isColorDialog() {
    return dialog.kind == DialogKind::AccentColor || dialog.kind == DialogKind::PanelColor;
}

bool isDialogOpen() {
    return dialog.kind != DialogKind::None;
}

/// @return Window title of the open dialog, also used as its ImGui popup ID.
static const char *dialogTitle() {
    switch ( dialog.kind ) {
    case DialogKind::Open:
        return "Open File";
    case DialogKind::Save:
        return "Save As";
    case DialogKind::AccentColor:
        return "Accent Color";
    case DialogKind::PanelColor:
        return "Panel Background";
    case DialogKind::Discard:
    case DialogKind::None:
        break;
    }
    return "Unsaved Changes";
}

/// @brief Draws the Cancel button.
/// @return True if Cancel was clicked or Escape pressed.
static bool cancelPressed() {
    return ImGui::Button( "Cancel" ) || ImGui::IsKeyPressed( ImGuiKey_Escape );
}

/// @brief Draws the unsaved changes question.
/// @param[out] finish Set to the discard callback when the user chooses Discard.
/// @return True when the dialog should close.
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

/// @brief Draws a picker for one theme colour: preset swatches, the picker and a Reset button.
/// @param[in,out] color        Colour being edited.
/// @param[in]     defaultColor Colour Reset restores.
/// @param[in]     presets      Colours offered as swatches.
/// @return True if the colour changed.
static bool drawColorPicker( ImU32 &color, ImU32 defaultColor, std::span<const ImU32> presets ) {
    ImVec4 value = ImGui::ColorConvertU32ToFloat4( color );
    bool changed = false;
    for ( size_t i = 0; i < presets.size(); ++i ) {
        ImGui::PushID( static_cast<int>( i ) );
        if ( i > 0 ) {
            ImGui::SameLine();
        }
        ImVec4 preset = ImGui::ColorConvertU32ToFloat4( presets[i] );
        if ( ImGui::ColorButton( "##preset", preset, ImGuiColorEditFlags_NoTooltip ) ) {
            value = preset;
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::Spacing();

    ImGui::SetNextItemWidth( ImGui::GetFontSize() * 16 );
    changed |= ImGui::ColorPicker3( "##picker", &value.x, ImGuiColorEditFlags_PickerHueBar );
    if ( ImGui::Button( "Reset" ) ) {
        value = ImGui::ColorConvertU32ToFloat4( defaultColor );
        changed = true;
    }

    if ( changed ) {
        color = ImGui::ColorConvertFloat4ToU32( value );
    }
    return changed;
}

/// @brief Draws the accent colour picker and applies the colour at once.
/// @return True when the dialog should close.
static bool drawAccentColorDialog() {
    const ImU32 presets[] = {
        defaultAccentColor,
        IM_COL32( 53, 116, 240, 255 ),
        IM_COL32( 227, 140, 70, 255 ),
        IM_COL32( 168, 112, 220, 255 ),
        IM_COL32( 219, 92, 92, 255 ),
        IM_COL32( 42, 172, 184, 255 ),
    };
    ImU32 color = accentColor();
    if ( drawColorPicker( color, defaultAccentColor, presets ) ) {
        setAccentColor( color );
        ImGui::MarkIniSettingsDirty();
    }
    ImGui::Separator();
    return ImGui::Button( "Close" ) || ImGui::IsKeyPressed( ImGuiKey_Escape );
}

/// @brief Draws the panel background colour picker and applies the colour at once.
/// @return True when the dialog should close.
static bool drawPanelColorDialog() {
    const ImU32 presets[] = {
        defaultIslandColor,
        IM_COL32( 30, 31, 34, 255 ),
        IM_COL32( 0, 0, 0, 255 ),
    };
    ImU32 color = islandColor();
    if ( drawColorPicker( color, defaultIslandColor, presets ) ) {
        setIslandColor( color );
        ImGui::MarkIniSettingsDirty();
    }
    ImGui::Separator();
    return ImGui::Button( "Close" ) || ImGui::IsKeyPressed( ImGuiKey_Escape );
}

/// @brief Draws the list of the current directory. A click on a file fills the name field.
/// @param[out] nextDirectory Set to the directory the user clicked; unchanged otherwise.
/// @return True if a file was double-clicked.
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

/// @brief Draws the Open or Save As dialog: file list, name field and buttons.
/// @param[out] finish Set to call the chosen-file callback when a file is accepted.
/// @return True when the dialog should close.
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
    outlineLastItem( ImGui::IsItemActive() );
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
    bool visible = isColorDialog() ? ImGui::BeginPopup( title, ImGuiWindowFlags_AlwaysAutoResize )
                                   : ImGui::BeginPopupModal( title, nullptr,
                                                             ImGuiWindowFlags_AlwaysAutoResize );
    if ( !visible ) {
        dialog = Dialog();
        return;
    }
    std::function<void()> finish;
    bool done = false;
    if ( dialog.kind == DialogKind::AccentColor ) {
        done = drawAccentColorDialog();
    } else if ( dialog.kind == DialogKind::PanelColor ) {
        done = drawPanelColorDialog();
    } else if ( dialog.kind == DialogKind::Discard ) {
        done = drawDiscardDialog( finish );
    } else {
        done = drawFileDialog( finish );
    }
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
