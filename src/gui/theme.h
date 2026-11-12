#pragma once

#include <TextEditor.h>
#include <imgui.h>

// Islands Dark: every panel is a rounded island on a darker ground.
inline constexpr ImU32 hoveredColor = IM_COL32( 38, 40, 43, 255 );
inline constexpr ImU32 selectedColor = IM_COL32( 43, 45, 48, 255 );
inline constexpr ImU32 borderColor = IM_COL32( 57, 59, 64, 255 );
inline constexpr ImU32 textSelectionColor = IM_COL32( 33, 66, 131, 255 );
inline constexpr ImU32 transparentColor = IM_COL32( 0, 0, 0, 0 );

inline constexpr ImU32 textColor = IM_COL32( 223, 225, 229, 255 );
inline constexpr ImU32 mutedTextColor = IM_COL32( 134, 138, 145, 255 );
inline constexpr ImU32 secondaryTextColor = IM_COL32( 168, 171, 178, 255 );
inline constexpr ImU32 codeTextColor = IM_COL32( 188, 190, 196, 255 );

inline constexpr ImU32 errorColor = IM_COL32( 247, 84, 100, 255 );
inline constexpr ImU32 errorLineColor = IM_COL32( 247, 84, 100, 40 );
inline constexpr ImU32 warningColor = IM_COL32( 242, 197, 92, 255 );
inline constexpr ImU32 warningLineColor = IM_COL32( 242, 197, 92, 30 );
inline constexpr ImU32 successColor = IM_COL32( 106, 171, 115, 255 );
inline constexpr ImU32 commandColor = mutedTextColor;
inline constexpr ImU32 changedValueColor = IM_COL32( 242, 197, 92, 255 );
inline constexpr ImU32 changedRowColor = IM_COL32( 242, 197, 92, 32 );

inline constexpr ImU32 breakpointColor = IM_COL32( 219, 92, 92, 255 );
inline constexpr ImU32 breakpointHoverColor = IM_COL32( 219, 92, 92, 90 );

inline constexpr float islandRounding = 10.0f;
inline constexpr float islandGap = 6.0f;
inline constexpr float toolbarFramePaddingY = 9.0f;
inline constexpr float editorTabsGap = 6.0f;

// JetBrains green, the colour of the Run icon, until the user picks another one.
inline constexpr ImU32 defaultAccentColor = IM_COL32( 87, 150, 92, 255 );
// Background of every panel and of the code, until the user picks another one.
inline constexpr ImU32 defaultIslandColor = IM_COL32( 24, 25, 27, 255 );

void applyIslandsTheme();
// The accent marks selected tabs, switched-on buttons and panel outlines.
ImU32 accentColor();
// The accent faintly mixed into the island colour: fill of selected and hovered tabs.
ImU32 accentFillColor();
void setAccentColor( ImU32 color );
// The accent at low alpha: background of the line the debugger is paused on.
ImU32 pausedLineColor();
ImU32 islandColor();
// The frame around the islands (toolbar, status bar, tool strip, gaps): the island colour at
// 65% brightness, so the two levels keep their contrast whatever colour is picked.
ImU32 groundColor();
void setIslandColor( ImU32 color );
TextEditor::Palette editorPalette();
