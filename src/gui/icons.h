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

// Turns the SVG icons into textures, so it needs the OpenGL context.
void loadIcons();
ImTextureID iconTexture( Icon icon );
