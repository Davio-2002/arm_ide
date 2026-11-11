#pragma once

#include <TextEditor.h>
#include <imgui.h>

// Islands Dark: every panel is a rounded island on a darker ground.
inline constexpr ImU32 groundColor = IM_COL32( 19, 19, 20, 255 );
inline constexpr ImU32 islandColor = IM_COL32( 30, 31, 34, 255 );
inline constexpr ImU32 hoveredColor = IM_COL32( 38, 40, 43, 255 );
inline constexpr ImU32 selectedColor = IM_COL32( 43, 45, 48, 255 );
inline constexpr ImU32 borderColor = IM_COL32( 57, 59, 64, 255 );
// JetBrains green, the colours of the Run icon: selected tabs and switched-on buttons.
inline constexpr ImU32 activeColor = IM_COL32( 37, 54, 39, 255 );
inline constexpr ImU32 activeBorderColor = IM_COL32( 87, 150, 92, 255 );
inline constexpr ImU32 textSelectionColor = IM_COL32( 33, 66, 131, 255 );
inline constexpr ImU32 accentColor = IM_COL32( 53, 116, 240, 255 );
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

inline constexpr ImU32 pausedColor = IM_COL32( 84, 138, 247, 255 );
inline constexpr ImU32 pausedLineColor = IM_COL32( 84, 138, 247, 45 );
inline constexpr ImU32 breakpointColor = IM_COL32( 219, 92, 92, 255 );
inline constexpr ImU32 breakpointHoverColor = IM_COL32( 219, 92, 92, 90 );

inline constexpr float islandRounding = 10.0f;
inline constexpr float islandGap = 6.0f;
inline constexpr float toolbarFramePaddingY = 9.0f;
inline constexpr float editorTabsGap = 6.0f;

void applyIslandsTheme();
TextEditor::Palette editorPalette();
