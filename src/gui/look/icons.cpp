#include <GLFW/glfw3.h>

#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gui/look/icons.h>
#include <gui/look/theme.h>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

namespace {

// NOLINTBEGIN(modernize-avoid-c-arrays): byte lists from embed_as_bytes, sized by their #include.
const unsigned char buildSvg[] = {
#include <build_svg.inc>
    0 };
const unsigned char runSvg[] = {
#include <run_svg.inc>
    0 };
const unsigned char debugSvg[] = {
#include <debug_svg.inc>
    0 };
const unsigned char stopSvg[] = {
#include <stop_svg.inc>
    0 };
const unsigned char resumeSvg[] = {
#include <resume_svg.inc>
    0 };
const unsigned char stepOverSvg[] = {
#include <stepOver_svg.inc>
    0 };
const unsigned char asmFileSvg[] = {
#include <asmFile_svg.inc>
    0 };
const unsigned char consoleSvg[] = {
#include <console_svg.inc>
    0 };
const unsigned char clearSvg[] = {
#include <clear_svg.inc>
    0 };
const unsigned char registersSvg[] = {
#include <registers_svg.inc>
    0 };
const unsigned char memorySvg[] = {
#include <memory_svg.inc>
    0 };
// NOLINTEND(modernize-avoid-c-arrays)

struct IconSource {
    Icon icon;
    const unsigned char *svg;
};

/// A new icon: a row here, its SVG include above and a value in Icon.
constexpr auto iconSources = std::to_array<IconSource>( {
    { Icon::Build, buildSvg },
    { Icon::Run, runSvg },
    { Icon::Debug, debugSvg },
    { Icon::Stop, stopSvg },
    { Icon::Resume, resumeSvg },
    { Icon::StepOver, stepOverSvg },
    { Icon::AsmFile, asmFileSvg },
    { Icon::Console, consoleSvg },
    { Icon::Clear, clearSvg },
    { Icon::Registers, registersSvg },
    { Icon::Memory, memorySvg },
} );
std::array<GLuint, std::size( iconSources )> textures = {};
bool loaded = false;
bool loadedDark = false;

/// @brief Dark icon colours and their JetBrains light variants.
const auto lightColors = std::to_array<std::pair<const char *, const char *>>( {
    { "#CED0D6", "#6C707E" },
    { "#57965C", "#208A3C" },
    { "#253627", "#F2FCF3" },
    { "#DB5C5C", "#E55765" },
    { "#5E3838", "#FFF7F7" },
    { "#8C94F0", "#5E68D9" },
} );

/// @brief Renders an SVG to a square RGBA image.
/// @param[in] light True to swap in the light icon colours.
std::vector<unsigned char> rasterizeSvg( const unsigned char *svg, int size, bool light ) {
    std::string text = reinterpret_cast<const char *>( svg );
    if ( light ) {
        for ( const auto &[dark, lightColor] : lightColors ) {
            for ( size_t at = text.find( dark ); at != std::string::npos;
                  at = text.find( dark, at ) ) {
                text.replace( at, std::string_view( dark ).size(), lightColor );
            }
        }
    }
    NSVGimage *image = nsvgParse( text.data(), "px", 96.0f );
    NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
    std::vector<unsigned char> pixels( static_cast<size_t>( size ) * size * 4 );
    float const scale = static_cast<float>( size ) / image->width;
    nsvgRasterize( rasterizer, image, 0, 0, scale, pixels.data(), size, size, size * 4 );
    nsvgDeleteRasterizer( rasterizer );
    nsvgDelete( image );
    return pixels;
}

/// @return OpenGL texture of a square RGBA image.
GLuint createTexture( const std::vector<unsigned char> &pixels, int size ) {
    GLuint texture = 0;
    glGenTextures( 1, &texture );
    glBindTexture( GL_TEXTURE_2D, texture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                  pixels.data() );
    return texture;
}

} // anonymous namespace

void updateIcons() {
    bool const dark = currentScheme().dark;
    if ( loaded && dark == loadedDark ) {
        return;
    }
    if ( loaded ) {
        glDeleteTextures( static_cast<GLsizei>( textures.size() ), textures.data() );
    }
    int const size = static_cast<int>( iconSize );
    for ( const IconSource &source : iconSources ) {
        std::vector<unsigned char> const pixels = rasterizeSvg( source.svg, size, !dark );
        textures[static_cast<size_t>( source.icon )] = createTexture( pixels, size );
    }
    loaded = true;
    loadedDark = dark;
}

ImTextureID iconTexture( Icon icon ) {
    return static_cast<ImTextureID>( textures[static_cast<size_t>( icon )] );
}
