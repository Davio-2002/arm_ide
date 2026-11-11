#pragma once

class Session;
struct MenuActions;

void drawToolbarStart( const Session &session, MenuActions &actions );

void drawToolbarEnd( const Session &session, MenuActions &actions );
