#include <filesystem>
#include <test_support.h>

TEST_F( AssemblyTest, BuildsExecutable ) {
    const std::string source = writeSource( "exit.s", R"(
        .global _start
_start: mov x0, #0
        mov x8, #93
        svc #0
)" );

    const BuildResult build = buildProgram( source );

    EXPECT_TRUE( build.success );
    EXPECT_TRUE( std::filesystem::exists( build.executablePath ) );
}
