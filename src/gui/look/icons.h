#pragma once

#include <imgui.h>

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

/// @brief Rasterizes the SVG icons, again when the scheme turns dark or light. Needs the GL
///        context; called before each frame so no texture in use is deleted.
void updateIcons();

/// @return Texture for ImGui::Image.
ImTextureID iconTexture( Icon icon );
