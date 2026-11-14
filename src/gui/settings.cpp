#include <imgui.h>
#include <imgui_internal.h>

#include <string>

#include <app/documents.h>
#include <gui/fonts.h>
#include <gui/settings.h>
#include <gui/theme.h>

namespace {

std::string settingsPath;

/// @return Non-null, so ImGui calls readSettingsLine().
void *openSettingsEntry( ImGuiContext *, ImGuiSettingsHandler *, const char * ) {
    return &settingsPath;
}

/// @brief Applies one "Key=value" line: zoom, fonts or theme.
void readSettingsLine( ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line ) {
    std::string text = line;
    if ( text.starts_with( "Zoom=" ) ) {
        setCodeFontSize( std::strtof( line + 5, nullptr ) );
    } else if ( text.starts_with( "UiFont=" ) ) {
        setUiFontFamily( text.substr( 7 ) );
    } else if ( text.starts_with( "CodeFont=" ) ) {
        setCodeFontFamily( text.substr( 9 ) );
    } else if ( text.starts_with( "Theme=" ) ) {
        for ( size_t i = 0; i < std::size( themeModeNames ); i++ ) {
            if ( text.substr( 6 ) == themeModeNames[i] ) {
                setThemeMode( static_cast<ThemeMode>( i ) );
            }
        }
    } else if ( text.starts_with( "DarkScheme=" ) || text.starts_with( "LightScheme=" ) ) {
        const ColorScheme *scheme = findColorScheme( text.substr( text.find( '=' ) + 1 ) );
        if ( scheme && scheme->dark == text.starts_with( "Dark" ) ) {
            setColorScheme( *scheme );
        }
    } else if ( text.starts_with( "Accent=" ) ) {
        setAccentColor( static_cast<ImU32>( std::strtoul( line + 7, nullptr, 16 ) ) );
    }
}

/// @brief Writes the IDE's section of the settings file.
void writeSettings( ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buffer ) {
    buffer->appendf( "[%s][Settings]\n", handler->TypeName );
    buffer->appendf( "Zoom=%g\n", codeFontSize() );
    buffer->appendf( "UiFont=%s\n", uiFontFamily().c_str() );
    buffer->appendf( "CodeFont=%s\n", codeFontFamily().c_str() );
    buffer->appendf( "Theme=%s\n", themeModeNames[static_cast<size_t>( themeMode() )] );
    buffer->appendf( "DarkScheme=%s\n", darkColorScheme().name );
    buffer->appendf( "LightScheme=%s\n", lightColorScheme().name );
    if ( hasCustomAccent() ) {
        buffer->appendf( "Accent=%08X\n", accentColor() );
    }
    buffer->append( "\n" );
}

} // anonymous namespace

void loadSettings() {
    settingsPath = configDirectory() / "imgui.ini";
    ImGuiSettingsHandler handler;
    handler.TypeName = "ArmIde";
    handler.TypeHash = ImHashStr( "ArmIde" );
    handler.ReadOpenFn = openSettingsEntry;
    handler.ReadLineFn = readSettingsLine;
    handler.WriteAllFn = writeSettings;
    ImGui::AddSettingsHandler( &handler );

    ImGui::GetIO().IniFilename = settingsPath.c_str();
    ImGui::LoadIniSettingsFromDisk( settingsPath.c_str() );
}
