#pragma once

#include <functional>
#include <string>

class Session;

void drawMenuBar( Session &session );

void openDocument( Session &session, const std::string &path );
void whenChangesDiscarded( const Session &session, std::function<void()> action );
void openDocumentIfAllowed( Session &session, const std::string &path );
