#include <imgui.h>

#include <string>

#include <app/session.h>
#include <gui/panels/panels.h>
#include <gui/panels/registers.h>
#include <gui/theme.h>
#include <gui/window.h>

static ImVec4 changeColor( bool changed ) {
    return ImGui::ColorConvertU32ToFloat4( changed ? changedValueColor : codeTextColor );
}

static void drawRegisterRow( const std::string &name, uint64_t value, uint64_t previousValue ) {
    ImVec4 color = changeColor( value != previousValue );
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled( "%s", name.c_str() );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "0x%016llx", static_cast<unsigned long long>( value ) );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "%lld", static_cast<long long>( value ) );
}

static void drawFlags( const Registers &registers, const Registers &previous ) {
    const char *names[] = { "N", "Z", "C", "V" };
    for ( int i = 0; i < 4; i++ ) {
        int bit = 31 - i;
        bool value = ( registers.nzcv >> bit ) & 1;
        bool previousValue = ( previous.nzcv >> bit ) & 1;
        ImGui::TextColored( changeColor( value != previousValue ), "%s=%d", names[i], value );
        ImGui::SameLine( 0.0f, 16.0f );
    }
    ImGui::NewLine();
}

void drawRegistersPanel( const Session &session ) {
    beginPanel( "Registers" );
    drawPanelTitle( "Registers" );
    if ( !session.hasRegisters ) {
        ImGui::TextDisabled( "Start debugging with F10 or F8" );
        ImGui::End();
        return;
    }

    const Registers &registers = session.registers;
    const Registers &previous = session.previousRegisters;
    pushCodeFont();
    drawFlags( registers, previous );
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if ( ImGui::BeginTable( "registers", 3, flags ) ) {
        for ( int i = 0; i < 31; i++ ) {
            std::string name = "x" + std::to_string( i );
            if ( i == 29 ) {
                name += " fp";
            } else if ( i == 30 ) {
                name += " lr";
            }
            drawRegisterRow( name, registers.x[i], previous.x[i] );
        }
        drawRegisterRow( "sp", registers.sp, previous.sp );
        drawRegisterRow( "pc", registers.pc, previous.pc );
        ImGui::EndTable();
    }
    ImGui::PopFont();
    ImGui::End();
}
