#include <imgui.h>

#include <array>
#include <cstdio>
#include <format>
#include <string>

#include <app/session/session.h>
#include <gui/look/fonts.h>
#include <gui/look/widgets.h>
#include <gui/panels/panels.h>

namespace {

const uint64_t memoryRowSize = 8;
const uint64_t memoryRowCount = 64;

std::array<char, 64> memoryAddressInput = { 's', 'p' };

/// @brief Address, hex bytes and ASCII; unreadable bytes show "??".
void drawMemoryRow( Session &session, uint64_t rowAddress ) {
    std::string hex;
    std::string text;
    for ( uint64_t i = 0; i < memoryRowSize; i++ ) {
        uint8_t byte = 0;
        if ( session.readMemory( rowAddress + i, byte ) ) {
            hex += std::format( "{:02x} ", byte );
            text += byte >= 32 && byte < 127 ? static_cast<char>( byte ) : '.';
        } else {
            hex += "?? ";
            text += ' ';
        }
    }
    ImGui::TextDisabled( "%08llx", static_cast<unsigned long long>( rowAddress ) );
    ImGui::SameLine();
    ImGui::Text( "%s %s", hex.c_str(), text.c_str() );
}

} // anonymous namespace

void drawMemoryPanel( Session &session ) {
    beginPanel( "Memory", ImGuiWindowFlags_HorizontalScrollbar );
    drawPanelTitle( "Memory" );
    float const inputWidth = ImGui::GetContentRegionAvail().x * 0.45f;
    float const labelWidth = ImGui::CalcTextSize( "Address" ).x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine( ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - inputWidth -
                     labelWidth );
    ImGui::TextDisabled( "Address" );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( inputWidth );
    ImGui::InputTextWithHint( "##address", "x1, sp, 0x4100d8", memoryAddressInput.data(),
                              memoryAddressInput.size() );
    outlineLastItem( ImGui::IsItemActive() );

    uint64_t address = 0;
    if ( !session.isPaused() ) {
        ImGui::TextDisabled( "Pause the program to read memory" );
    } else if ( !parseAddress( memoryAddressInput.data(), session.paused.registers, address ) ) {
        ImGui::TextDisabled( "Type an address like 0x4100d8, or a register like x1 or sp" );
    } else {
        pushCodeFont();
        uint64_t const firstRow = address / memoryRowSize * memoryRowSize;
        for ( uint64_t row = 0; row < memoryRowCount; row++ ) {
            drawMemoryRow( session, firstRow + row * memoryRowSize );
        }
        ImGui::PopFont();
    }
    ImGui::End();
}
