#include <algorithm>
#include <filesystem>
#include <set>
#include <test_support.h>
#include <vector>

namespace {

const std::string exitProgram = ".global _start\n"      // 1
                                "_start: mov x0, #0\n"  // 2
                                "        mov x8, #93\n" // 3
                                "        svc #0\n";     // 4

const std::string cProgram = "int main( void ) {\n" // 1
                             "    int a = 1;\n"     // 2
                             "    int b = a + 2;\n" // 3
                             "    return b;\n"      // 4
                             "}\n";                 // 5

/// @return The source lines of the line table rows, in address order, without rows outside
///         the source.
std::vector<int> linesInAddressOrder( const std::map<uint64_t, int> &sourceLines ) {
    std::vector<int> lines;
    for ( const auto &[address, line] : sourceLines ) {
        if ( line != 0 ) {
            lines.push_back( line );
        }
    }
    return lines;
}

/// @return Whether some message is an error on this source line.
bool hasErrorOnLine( const std::vector<Message> &messages, int line ) {
    return std::ranges::any_of( messages, [line]( const Message &message ) {
        return message.kind == MessageKind::Error && message.sourceLine == line;
    } );
}

} // namespace

TEST_F( AssemblyTest, BuildsExecutable ) {
    const BuildResult build = buildProgram( writeSource( "exit.s", exitProgram ) );

    EXPECT_TRUE( build.success );
    EXPECT_TRUE( std::filesystem::exists( build.executablePath ) );
}

TEST_F( AssemblyTest, ReportsErrorOnItsLine ) {
    const std::string source = ".global _start\n"         // 1
                               "_start: mov x0, #0\n"     // 2
                               "        frobnicate x0\n"; // 3

    const BuildResult build = buildProgram( writeSource( "bad.s", source ) );

    EXPECT_FALSE( build.success );
    EXPECT_TRUE( hasErrorOnLine( build.messages, 3 ) );
}

TEST_F( AssemblyTest, GivesEachInstructionItsOwnLine ) {
    const std::string source = writeSource( "exit.s", exitProgram );
    const BuildResult build = buildProgram( source );
    ASSERT_TRUE( build.success );

    const auto lines = linesInAddressOrder( readSourceLines( build.executablePath, source ) );

    EXPECT_EQ( lines, ( std::vector<int>{ 2, 3, 4 } ) );
}

TEST_F( CTest, BuildsExecutable ) {
    const BuildResult build = buildProgram( writeSource( "main.c", cProgram ) );

    EXPECT_TRUE( build.success );
    EXPECT_TRUE( std::filesystem::exists( build.executablePath ) );
}

TEST_F( CTest, ReportsErrorOnItsLine ) {
    const std::string source = "int main( void ) {\n"     // 1
                               "    return undeclared;\n" // 2
                               "}\n";                     // 3

    const BuildResult build = buildProgram( writeSource( "bad.c", source ) );

    EXPECT_FALSE( build.success );
    EXPECT_TRUE( hasErrorOnLine( build.messages, 2 ) );
}

TEST_F( CTest, ReadsLinesOfMain ) {
    const std::string source = writeSource( "main.c", cProgram );
    const BuildResult build = buildProgram( source );
    ASSERT_TRUE( build.success );

    const auto lines = linesInAddressOrder( readSourceLines( build.executablePath, source ) );
    const std::set<int> distinctLines( lines.begin(), lines.end() );

    EXPECT_EQ( distinctLines, ( std::set<int>{ 1, 2, 3, 4, 5 } ) );
}

TEST_F( CTest, FindsMainSymbol ) {
    const BuildResult build = buildProgram( writeSource( "main.c", cProgram ) );
    ASSERT_TRUE( build.success );

    EXPECT_NE( symbolAddress( build.executablePath, "main" ), 0 );
    EXPECT_EQ( symbolAddress( build.executablePath, "no_such_symbol" ), 0 );
}
