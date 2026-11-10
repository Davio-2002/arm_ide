#pragma once

#include <string>

struct GLFWwindow;
class Session;

GLFWwindow *createWindow();
void setupImGui( GLFWwindow *window, Session &session );
void shutdownWindow( GLFWwindow *window );

void beginFrame();
void endFrame( GLFWwindow *window );
void setWindowTitle( GLFWwindow *window, const std::string &title );
std::string takeDroppedFile();

void pushCodeFont();
void zoomCode( float step );
void resetZoom();
