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

/// @brief Rasterizes the embedded SVG icons into textures, again when the scheme switches between
///        dark and light (light schemes get the light colours of the JetBrains icons).
///
/// Needs the OpenGL context. Called before each frame, so no texture is deleted while the
/// frame's draw data uses it.
void updateIcons();

/// @param[in] icon Icon to draw.
/// @return Texture of the icon, for ImGui::Image.
ImTextureID iconTexture( Icon icon );
