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
    bool changed = value != previousValue;
    ImVec4 color = changeColor( changed );
    ImGui::TableNextRow();
    if ( changed ) {
        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, changedRowColor );
    }
    ImGui::TableNextColumn();
    ImGui::TextDisabled( "%s", name.c_str() );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "0x%016llx", static_cast<unsigned long long>( value ) );
    ImGui::TableNextColumn();
    ImGui::TextColored( color, "%lld", static_cast<long long>( value ) );
}

struct Flags {
    bool n, z, c, v;
};

static Flags readFlags( uint64_t nzcv ) {
    return { ( ( nzcv >> 31 ) & 1 ) != 0, ( ( nzcv >> 30 ) & 1 ) != 0, ( ( nzcv >> 29 ) & 1 ) != 0,
             ( ( nzcv >> 28 ) & 1 ) != 0 };
}

// A rounded label, filled while `on`; wraps to the next line when the row is full.
static void drawPill( const char *text, bool on, ImU32 textColor, const char *tooltip,
                      float rowRight, bool first ) {
    ImVec2 padding( 6.0f, 2.0f );
    ImVec2 textSize = ImGui::CalcTextSize( text );
    ImVec2 size( textSize.x + padding.x * 2, textSize.y + padding.y * 2 );
    float spacing = 4.0f;
    if ( !first && ImGui::GetItemRectMax().x + spacing + size.x <= rowRight ) {
        ImGui::SameLine( 0.0f, spacing );
    }
    ImGui::InvisibleButton( text, size );
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float rounding = ImGui::GetStyle().FrameRounding;
    if ( on ) {
        drawList->AddRectFilled( min, max, accentFillColor(), rounding );
        drawList->AddRect( min, max, accentColor(), rounding );
    }
    drawList->AddText( ImVec2( min.x + padding.x, min.y + padding.y ), textColor, text );
    ImGui::SetItemTooltip( "%s", tooltip );
}

static void drawFlags( const Registers &registers, const Registers &previous ) {
    struct Flag {
        const char *label;
        int bit;
        const char *meaning;
    };
    const Flag flags[] = {
        { "N", 31, "Negative: the top bit of the last flag-setting result is 1" },
        { "Z", 30, "Zero: the result was zero (after cmp a, b: a == b)" },
        { "C", 29,
          "Carry: an add overflowed as unsigned, or a subtract did not borrow\n"
          "(after cmp a, b: a >= b unsigned)" },
        { "V", 28, "Overflow: the result did not fit as a signed number" },
    };
    float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool first = true;
    for ( const Flag &flag : flags ) {
        bool value = ( registers.nzcv >> flag.bit ) & 1;
        bool previousValue = ( previous.nzcv >> flag.bit ) & 1;
        std::string text = std::string( flag.label ) + "=" + ( value ? "1" : "0" );
        ImU32 color = value != previousValue ? changedValueColor : codeTextColor;
        drawPill( text.c_str(), value, color, flag.meaning, rowRight, first );
        first = false;
    }
}

// The condition codes of b.cond, csel and friends that hold for the current flags.
static void drawConditions( uint64_t nzcv ) {
    Flags f = readFlags( nzcv );
    struct Condition {
        const char *name;
        bool holds;
        const char *meaning;
    };
    const Condition conditions[] = {
        { "eq", f.z, "equal: Z=1" },
        { "ne", !f.z, "not equal: Z=0" },
        { "hs", f.c, "unsigned higher or same (cs): C=1" },
        { "lo", !f.c, "unsigned lower (cc): C=0" },
        { "hi", f.c && !f.z, "unsigned higher: C=1 and Z=0" },
        { "ls", !( f.c && !f.z ), "unsigned lower or same: C=0 or Z=1" },
        { "ge", f.n == f.v, "signed greater or equal: N=V" },
        { "lt", f.n != f.v, "signed less than: N!=V" },
        { "gt", !f.z && f.n == f.v, "signed greater than: Z=0 and N=V" },
        { "le", f.z || f.n != f.v, "signed less or equal: Z=1 or N!=V" },
        { "mi", f.n, "minus, negative: N=1" },
        { "pl", !f.n, "plus, positive or zero: N=0" },
        { "vs", f.v, "signed overflow: V=1" },
        { "vc", !f.v, "no signed overflow: V=0" },
    };
    float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool first = true;
    for ( const Condition &condition : conditions ) {
        ImU32 color = condition.holds ? codeTextColor : mutedTextColor;
        drawPill( condition.name, condition.holds, color, condition.meaning, rowRight, first );
        first = false;
    }
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
    drawConditions( registers.nzcv );
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
