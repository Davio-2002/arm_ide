#include <GLFW/glfw3.h>

#include <array>
#include <iterator>
#include <string>
#include <vector>

#include <gui/icons.h>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

static const unsigned char buildSvg[] = {
#include "build_svg.inc"
    0 };
static const unsigned char runSvg[] = {
#include "run_svg.inc"
    0 };
static const unsigned char debugSvg[] = {
#include "debug_svg.inc"
    0 };
static const unsigned char stopSvg[] = {
#include "stop_svg.inc"
    0 };
static const unsigned char resumeSvg[] = {
#include "resume_svg.inc"
    0 };
static const unsigned char stepOverSvg[] = {
#include "stepOver_svg.inc"
    0 };
static const unsigned char asmFileSvg[] = {
#include "asmFile_svg.inc"
    0 };
static const unsigned char consoleSvg[] = {
#include "console_svg.inc"
    0 };
static const unsigned char clearSvg[] = {
#include "clear_svg.inc"
    0 };
static const unsigned char registersSvg[] = {
#include "registers_svg.inc"
    0 };
static const unsigned char memorySvg[] = {
#include "memory_svg.inc"
    0 };

static const Icon allIcons[] = { Icon::Build,  Icon::Run,       Icon::Debug,   Icon::Stop,
                                 Icon::Resume, Icon::StepOver,  Icon::AsmFile, Icon::Console,
                                 Icon::Clear,  Icon::Registers, Icon::Memory };
static std::array<GLuint, std::size( allIcons )> textures = {};

static const unsigned char *iconSvg( Icon icon ) {
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

static std::vector<unsigned char> rasterizeSvg( const unsigned char *svg, int size ) {
    std::string text = reinterpret_cast<const char *>( svg ); // nsvgParse writes into its input
    NSVGimage *image = nsvgParse( text.data(), "px", 96.0f );
    NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
    std::vector<unsigned char> pixels( size * size * 4 );
    float scale = size / image->width;
    nsvgRasterize( rasterizer, image, 0, 0, scale, pixels.data(), size, size, size * 4 );
    nsvgDeleteRasterizer( rasterizer );
    nsvgDelete( image );
    return pixels;
}

static GLuint createTexture( const std::vector<unsigned char> &pixels, int size ) {
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

void loadIcons() {
    int size = static_cast<int>( iconSize );
    for ( Icon icon : allIcons ) {
        std::vector<unsigned char> pixels = rasterizeSvg( iconSvg( icon ), size );
        textures[static_cast<size_t>( icon )] = createTexture( pixels, size );
    }
}

ImTextureID iconTexture( Icon icon ) {
    return static_cast<ImTextureID>( textures[static_cast<size_t>( icon )] );
}
