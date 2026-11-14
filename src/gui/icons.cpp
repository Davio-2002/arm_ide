#include <GLFW/glfw3.h>

#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gui/icons.h>
#include <gui/theme.h>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

namespace {

const unsigned char buildSvg[] = {
#include "build_svg.inc"
    0 };
const unsigned char runSvg[] = {
#include "run_svg.inc"
    0 };
const unsigned char debugSvg[] = {
#include "debug_svg.inc"
    0 };
const unsigned char stopSvg[] = {
#include "stop_svg.inc"
    0 };
const unsigned char resumeSvg[] = {
#include "resume_svg.inc"
    0 };
const unsigned char stepOverSvg[] = {
#include "stepOver_svg.inc"
    0 };
const unsigned char asmFileSvg[] = {
#include "asmFile_svg.inc"
    0 };
const unsigned char consoleSvg[] = {
#include "console_svg.inc"
    0 };
const unsigned char clearSvg[] = {
#include "clear_svg.inc"
    0 };
const unsigned char registersSvg[] = {
#include "registers_svg.inc"
    0 };
const unsigned char memorySvg[] = {
#include "memory_svg.inc"
    0 };

const Icon allIcons[] = { Icon::Build,  Icon::Run,       Icon::Debug,   Icon::Stop,
                          Icon::Resume, Icon::StepOver,  Icon::AsmFile, Icon::Console,
                          Icon::Clear,  Icon::Registers, Icon::Memory };
std::array<GLuint, std::size( allIcons )> textures = {};
bool loaded = false;
bool loadedDark = false;

/// @brief Colours of the dark icons and the ones the light variants of JetBrains icons use.
const std::pair<const char *, const char *> lightColors[] = {
    { "#CED0D6", "#6C707E" }, { "#57965C", "#208A3C" }, { "#253627", "#F2FCF3" },
    { "#DB5C5C", "#E55765" }, { "#5E3838", "#FFF7F7" }, { "#8C94F0", "#5E68D9" },
};

/// @param[in] icon Icon.
/// @return Null-terminated SVG source of the icon.
const unsigned char *iconSvg( Icon icon ) {
    switch ( icon ) {
    case Icon::Build:
        return buildSvg;
    case Icon::Run:
        return runSvg;
    case Icon::Debug:
        return debugSvg;
    case Icon::Stop:
        return stopSvg;
    case Icon::Resume:
        return resumeSvg;
    case Icon::StepOver:
        return stepOverSvg;
    case Icon::AsmFile:
        return asmFileSvg;
    case Icon::Console:
        return consoleSvg;
    case Icon::Clear:
        return clearSvg;
    case Icon::Registers:
        return registersSvg;
    case Icon::Memory:
        break;
    }
    return memorySvg;
}

/// @brief Renders an SVG to a square RGBA image.
/// @param[in] svg   Null-terminated SVG source.
/// @param[in] size  Image width and height in pixels.
/// @param[in] light True to swap the colours for those of light icons.
/// @return RGBA pixels, row by row.
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
    std::vector<unsigned char> pixels( size * size * 4 );
    float scale = size / image->width;
    nsvgRasterize( rasterizer, image, 0, 0, scale, pixels.data(), size, size, size * 4 );
    nsvgDeleteRasterizer( rasterizer );
    nsvgDelete( image );
    return pixels;
}

/// @brief Uploads a square RGBA image as an OpenGL texture.
/// @param[in] pixels RGBA pixels, row by row.
/// @param[in] size   Image width and height in pixels.
/// @return Texture name.
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
    bool dark = currentScheme().dark;
    if ( loaded && dark == loadedDark ) {
        return;
    }
    if ( loaded ) {
        glDeleteTextures( static_cast<GLsizei>( textures.size() ), textures.data() );
    }
    int size = static_cast<int>( iconSize );
    for ( Icon icon : allIcons ) {
        std::vector<unsigned char> pixels = rasterizeSvg( iconSvg( icon ), size, !dark );
        textures[static_cast<size_t>( icon )] = createTexture( pixels, size );
    }
    loaded = true;
    loadedDark = dark;
}

ImTextureID iconTexture( Icon icon ) {
    return static_cast<ImTextureID>( textures[static_cast<size_t>( icon )] );
}
