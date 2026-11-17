#include <imgui.h>

#include <array>
#include <string>

#include <app/session.h>
#include <gui/fonts.h>
#include <gui/panels/panels.h>
#include <gui/theme.h>

namespace {

ImVec4 changeColor( bool changed ) {
    return ImGui::ColorConvertU32ToFloat4( changed ? currentScheme().changedValue
                                                   : currentScheme().codeText );
}

/// @brief Tooltip with the value unsigned, signed and as its w register.
/// @param[in] lowName 32-bit view name, or nullptr (pc).
void drawValueTooltip( const std::string &name, const char *lowName, uint64_t value ) {
    if ( !ImGui::BeginItemTooltip() ) {
        return;
    }
    ImGui::Text( "%s  0x%016llx", name.c_str(), static_cast<unsigned long long>( value ) );
    ImGui::TextDisabled( "  unsigned %llu", static_cast<unsigned long long>( value ) );
    ImGui::TextDisabled( "  signed   %lld", static_cast<long long>( value ) );
    if ( lowName != nullptr ) {
        auto const low = static_cast<uint32_t>( value );
        ImGui::Text( "%s  0x%08x", lowName, low );
        ImGui::TextDisabled( "  unsigned %u", low );
        ImGui::TextDisabled( "  signed   %d", static_cast<int32_t>( low ) );
    }
    ImGui::EndTooltip();
}

/// @brief Name, hex and signed value; highlighted if changed since the last pause.
void drawRegisterRow( const std::string &name, const char *lowName, uint64_t value,
                      uint64_t previousValue ) {
    bool const changed = value != previousValue;
    ImVec4 const color = changeColor( changed );
    ImGui::TableNextRow();
    if ( changed ) {
        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                withAlpha( currentScheme().changedValue, 32 ) );
    }
    ImGui::TableNextColumn();
    ImGui::TextDisabled( "%s", name.c_str() );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "0x%016llx", static_cast<unsigned long long>( value ) );
    drawValueTooltip( name, lowName, value );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "%lld", static_cast<long long>( value ) );
    drawValueTooltip( name, lowName, value );
}

struct Flags {
    bool n, z, c, v;
};

/// @param[in] nzcv Flags in bits 28 to 31.
Flags readFlags( uint64_t nzcv ) {
    return { ( ( nzcv >> 31 ) & 1 ) != 0, ( ( nzcv >> 30 ) & 1 ) != 0, ( ( nzcv >> 29 ) & 1 ) != 0,
             ( ( nzcv >> 28 ) & 1 ) != 0 };
}

/// @brief Rounded label that wraps to the next line when the row is full.
/// @param[in] on       True to fill and outline it in the accent.
/// @param[in] rowRight Right edge of the row.
void drawPill( const char *text, bool on, ImU32 textColor, const char *tooltip, float rowRight,
               bool first ) {
    ImVec2 const padding( 6.0f, 2.0f );
    ImVec2 const textSize = ImGui::CalcTextSize( text );
    ImVec2 const size( textSize.x + padding.x * 2, textSize.y + padding.y * 2 );
    float const spacing = 4.0f;
    if ( !first && ImGui::GetItemRectMax().x + spacing + size.x <= rowRight ) {
        ImGui::SameLine( 0.0f, spacing );
    }
    ImGui::InvisibleButton( text, size );
    ImVec2 const min = ImGui::GetItemRectMin();
    ImVec2 const max = ImGui::GetItemRectMax();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float const rounding = ImGui::GetStyle().FrameRounding;
    if ( on ) {
        drawList->AddRectFilled( min, max, accentFillColor(), rounding );
        drawList->AddRect( min, max, accentColor(), rounding );
    }
    drawList->AddText( ImVec2( min.x + padding.x, min.y + padding.y ), textColor, text );
    ImGui::SetItemTooltip( "%s", tooltip );
}

/// @brief N, Z, C and V with their meaning as tooltip; changed ones highlighted.
void drawFlags( const Registers &registers, const Registers &previous ) {
    struct Flag {
        const char *label;
        int bit;
        const char *meaning;
    };
    const auto flags = std::to_array<Flag>( {
        { "N", 31, "Negative: the top bit of the last flag-setting result is 1" },
        { "Z", 30, "Zero: the result was zero (after cmp a, b: a == b)" },
        { "C", 29,
          "Carry: an add overflowed as unsigned, or a subtract did not borrow\n"
          "(after cmp a, b: a >= b unsigned)" },
        { "V", 28, "Overflow: the result did not fit as a signed number" },
    } );
    float const rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool first = true;
    for ( const Flag &flag : flags ) {
        bool const value = ( registers.nzcv >> flag.bit ) & 1;
        bool const previousValue = ( previous.nzcv >> flag.bit ) & 1;
        std::string const text = std::string( flag.label ) + "=" + ( value ? "1" : "0" );
        ImU32 const color =
            value != previousValue ? currentScheme().changedValue : currentScheme().codeText;
        drawPill( text.c_str(), value, color, flag.meaning, rowRight, first );
        first = false;
    }
}

/// @brief Condition codes of b.cond, csel etc., highlighting those that hold.
void drawConditions( uint64_t nzcv ) {
    Flags f = readFlags( nzcv );
    struct Condition {
        const char *name;
        bool holds;
        const char *meaning;
    };
    const auto conditions = std::to_array<Condition>( {
        { "eq", f.z, "equal: Z=1" },
        { "ne", !f.z, "not equal: Z=0" },
        { "hs", f.c, "unsigned higher or same (cs): C=1" },
        { "lo", !f.c, "unsigned lower (cc): C=0" },
        { "hi", f.c && !f.z, "unsigned higher: C=1 and Z=0" },
        { "ls", !f.c || f.z, "unsigned lower or same: C=0 or Z=1" },
        { "ge", f.n == f.v, "signed greater or equal: N=V" },
        { "lt", f.n != f.v, "signed less than: N!=V" },
        { "gt", !f.z && f.n == f.v, "signed greater than: Z=0 and N=V" },
        { "le", f.z || f.n != f.v, "signed less or equal: Z=1 or N!=V" },
        { "mi", f.n, "minus, negative: N=1" },
        { "pl", !f.n, "plus, positive or zero: N=0" },
        { "vs", f.v, "signed overflow: V=1" },
        { "vc", !f.v, "no signed overflow: V=0" },
    } );
    float const rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool first = true;
    for ( const Condition &condition : conditions ) {
        ImU32 const color = condition.holds ? currentScheme().codeText : currentScheme().mutedText;
        drawPill( condition.name, condition.holds, color, condition.meaning, rowRight, first );
        first = false;
    }
}

} // anonymous namespace

void drawRegistersPanel( const Session &session ) {
    beginPanel( "Registers" );
    drawPanelTitle( "Registers" );
    if ( !session.paused.hasRegisters ) {
        ImGui::TextDisabled( "Start debugging with F10 or F8" );
        ImGui::End();
        return;
    }

    const Registers &registers = session.paused.registers;
    const Registers &previous = session.paused.previousRegisters;
    pushCodeFont();
    drawFlags( registers, previous );
    drawConditions( registers.nzcv );
    ImGuiTableFlags const flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if ( ImGui::BeginTable( "registers", 3, flags ) ) {
        for ( int i = 0; i < 31; i++ ) {
            std::string name = "x" + std::to_string( i );
            std::string const lowName = "w" + std::to_string( i );
            if ( i == 29 ) {
                name += " fp";
            } else if ( i == 30 ) {
                name += " lr";
            }
            drawRegisterRow( name, lowName.c_str(), registers.x[i], previous.x[i] );
        }
        drawRegisterRow( "sp", "wsp", registers.sp, previous.sp );
        drawRegisterRow( "pc", nullptr, registers.pc, previous.pc );
        ImGui::EndTable();
    }
    ImGui::PopFont();
    ImGui::End();
}
