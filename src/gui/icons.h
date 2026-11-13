#pragma once

#include <imgui.h>

/// @brief Icons of the toolbar, panels and tool strip.
enum class Icon {
    Build,
    Run,
    Debug,
    Stop,
    Resume,
    StepOver,
    AsmFile,
    Console,
    Clear,
    Registers,
    Memory
};

inline constexpr float iconSize = 20.0f;

/// @brief Rasterizes the embedded SVG icons into textures.
///
/// Needs the OpenGL context, so it is called after the window is created.
void loadIcons();

/// @param[in] icon Icon to draw.
/// @return Texture of the icon, for ImGui::Image.
ImTextureID iconTexture( Icon icon );
