#include <imgui.h>
#include <imgui_internal.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include <app/session.h>
#include <gui/fonts.h>
#include <gui/settings.h>
#include <gui/theme.h>

namespace {

std::string settingsPath;
Session *settingsSession = nullptr;

/// @brief ImGui settings callback: starts reading the IDE's section of the settings file.
/// @return The session, passed to readSettingsLine() as the entry.
void *openSettingsEntry( ImGuiContext *, ImGuiSettingsHandler *, const char * ) {
    settingsSession->recentFiles.clear();
    return settingsSession;
}

/// @brief ImGui settings callback: applies one line of the IDE's section: zoom, fonts, theme or
///        a recent file that still exists.
/// @param[in] line Line in "Key=value" form.
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
    } else if ( text.starts_with( "Recent=" ) && std::filesystem::exists( text.substr( 7 ) ) ) {
        settingsSession->recentFiles.push_back( text.substr( 7 ) );
    }
}

/// @brief ImGui settings callback: writes the IDE's section of the settings file.
/// @param[in]     handler The IDE's settings handler.
/// @param[in,out] buffer  Settings text being written.
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
    for ( const std::string &path : settingsSession->recentFiles ) {
        buffer->appendf( "Recent=%s\n", path.c_str() );
    }
    buffer->append( "\n" );
}

} // anonymous namespace

void loadSettings( Session &session ) {
    const char *configHome = std::getenv( "XDG_CONFIG_HOME" );
    const char *home = std::getenv( "HOME" );
    std::filesystem::path configDirectory =
        configHome ? configHome : std::string( home ? home : "." ) + "/.config";
    configDirectory /= "arm_ide";
    std::error_code ignored;
    std::filesystem::create_directories( configDirectory, ignored );
    settingsPath = configDirectory / "imgui.ini";

    settingsSession = &session;
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
