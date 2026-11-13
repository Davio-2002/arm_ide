#pragma once

#include <string>

struct GLFWwindow;
class Session;

/// @brief Initializes GLFW and creates the main window with an OpenGL 3.3 context.
/// @return The window, or nullptr on failure.
GLFWwindow *createWindow();

/// @brief Creates the ImGui context: fonts, theme, settings file and input callbacks.
/// @param[in]     window  Main window.
/// @param[in,out] session Session whose recent files are loaded and saved with the settings.
void setupImGui( GLFWwindow *window, Session &session );

/// @brief Destroys ImGui, the window and GLFW.
/// @param[in] window Main window.
void shutdownWindow( GLFWwindow *window );

/// @brief Starts a new ImGui frame.
void beginFrame();

/// @brief Renders the ImGui frame on the ground colour and shows it.
/// @param[in] window Main window.
void endFrame( GLFWwindow *window );

/// @brief Sets the window title if it changed.
/// @param[in] window Main window.
/// @param[in] title  New title.
void setWindowTitle( GLFWwindow *window, const std::string &title );

/// @brief Takes the file last dropped onto the window.
/// @return Its path, or an empty string if none was dropped since the last call.
std::string takeDroppedFile();

/// @brief Pushes the code font at the current zoom. Undo with ImGui::PopFont.
void pushCodeFont();

/// @brief Changes the code font size, within limits.
/// @param[in] step Points to add; negative to zoom out.
void zoomCode( float step );

/// @brief Restores the default code font size.
void resetZoom();
