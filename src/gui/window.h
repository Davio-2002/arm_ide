#pragma once

#include <string>

struct GLFWwindow;

/// @brief Initializes GLFW and creates the main window with an OpenGL 3.3 context.
/// @return The window, or nullptr on failure.
GLFWwindow *createWindow();

/// @brief Creates the ImGui context: fonts, theme, settings file and input callbacks.
void setupImGui( GLFWwindow *window );

/// @brief Destroys ImGui, the window and GLFW.
void shutdownWindow( GLFWwindow *window );

/// @brief Waits for input, or until the next frame is due: at once while busy or just after
///        input, otherwise after the idle wait.
/// @param[in] busy True while something outside the window changes (program, debugger,
///                 terminal output).
void waitForNextFrame( bool busy );

/// @brief Starts a new ImGui frame.
void beginFrame();

/// @brief Renders the ImGui frame on the ground colour and shows it.
void endFrame( GLFWwindow *window );

/// @brief Sets the window title if it changed.
/// @param[in] title  New title.
void setWindowTitle( GLFWwindow *window, const std::string &title );

/// @brief Takes the file last dropped onto the window.
/// @return Its path, or an empty string if none was dropped since the last call.
std::string takeDroppedFile();
