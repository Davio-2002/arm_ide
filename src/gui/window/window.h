#pragma once

#include <string>

struct GLFWwindow;

/// @brief Creates the main window with an OpenGL 3.3 context.
/// @return nullptr on failure.
GLFWwindow *createWindow();

/// @brief Creates the ImGui context: fonts, theme, settings and input callbacks.
void setupImGui( GLFWwindow *window );

void shutdownWindow( GLFWwindow *window );

/// @brief Waits for input or the next frame: at once while busy or just after input.
/// @param[in] busy True while a program, the debugger or the terminal changes things.
void waitForNextFrame( bool busy );

void beginFrame();

/// @brief Renders the frame on the ground colour.
void endFrame( GLFWwindow *window );

/// @brief Sets the title if it changed.
void setWindowTitle( GLFWwindow *window, const std::string &title );

/// @return Path dropped since the last call, or empty.
std::string takeDroppedFile();
