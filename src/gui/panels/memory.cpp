#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <app/session.h>
#include <gui/panels/memory.h>
#include <gui/panels/panels.h>
#include <gui/widgets.h>
#include <gui/window.h>

static const uint64_t memoryRowSize = 8;
static const uint64_t memoryRowCount = 64;

static char memoryAddressInput[64] = "sp";

/// @brief Parses the address field.
/// @param[in]  text      "sp", "pc", a register x0 to x30, or a number in C notation.
/// @param[in]  registers Current register values.
/// @param[out] address   Parsed address.
/// @return True if the text is valid.
static bool parseAddress( const std::string &text, const Registers &registers, uint64_t &address ) {
    if ( text == "sp" ) {
        address = registers.sp;
        return true;
    }
    if ( text == "pc" ) {
        address = registers.pc;
        return true;
    }

    const char *start = text.c_str();
    bool isRegister = text.starts_with( 'x' );
    if ( isRegister ) {
        start++;
    }
    char *end = nullptr;
    uint64_t value = std::strtoull( start, &end, isRegister ? 10 : 0 );
    if ( end == start || *end != '\0' ) {
        return false;
    }

    if ( !isRegister ) {
        address = value;
        return true;
    }
    if ( value > 30 ) {
        return false;
    }
    address = registers.x[value];
    return true;
}

/// @brief Draws one row: address, bytes in hex and as ASCII. Unreadable bytes show "??".
/// @param[in,out] session    Session that reads the memory.
/// @param[in]     rowAddress Address of the first byte.
static void drawMemoryRow( Session &session, uint64_t rowAddress ) {
    std::string hex;
    std::string text;
    char byteText[4];
    for ( uint64_t i = 0; i < memoryRowSize; i++ ) {
        uint8_t byte = 0;
        if ( session.readMemory( rowAddress + i, byte ) ) {
            std::snprintf( byteText, sizeof( byteText ), "%02x ", byte );
            hex += byteText;
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

void drawMemoryPanel( Session &session ) {
    beginPanel( "Memory", ImGuiWindowFlags_HorizontalScrollbar );
    drawPanelTitle( "Memory" );
    float inputWidth = ImGui::GetContentRegionAvail().x * 0.45f;
    float labelWidth = ImGui::CalcTextSize( "Address" ).x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine( ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - inputWidth -
                     labelWidth );
    ImGui::TextDisabled( "Address" );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( inputWidth );
    ImGui::InputTextWithHint( "##address", "x1, sp, 0x4100d8", memoryAddressInput,
                              sizeof( memoryAddressInput ) );
    outlineLastItem( ImGui::IsItemActive() );

    uint64_t address = 0;
    if ( !session.isPaused() ) {
        ImGui::TextDisabled( "Pause the program to read memory" );
    } else if ( !parseAddress( memoryAddressInput, session.registers, address ) ) {
        ImGui::TextDisabled( "Type an address like 0x4100d8, or a register like x1 or sp" );
    } else {
        pushCodeFont();
        uint64_t firstRow = address / memoryRowSize * memoryRowSize;
        for ( uint64_t row = 0; row < memoryRowCount; row++ ) {
            drawMemoryRow( session, firstRow + row * memoryRowSize );
        }
        ImGui::PopFont();
    }
    ImGui::End();
}
