#include <fstream>
#include <stdlib.h>
#include <test_support.h>

#include <core/process.h>

void ProgramTest::SetUp() {
    std::string pattern = ( std::filesystem::temp_directory_path() / "arm_ide_test_XXXXXX" );
    ASSERT_NE( mkdtemp( pattern.data() ), nullptr ) << "can't create a temporary folder";
    folder = pattern;
}

void ProgramTest::TearDown() {
    std::error_code error;
    std::filesystem::remove_all( folder, error );
}

std::string ProgramTest::writeSource( const std::string &name, const std::string &text ) const {
    const std::filesystem::path path = folder / name;
    std::ofstream( path ) << text;
    return path;
}

namespace {

/// @return The first tool a test needs that is not installed, or empty.
std::string missingTool( SourceLanguage language, bool needsQemu ) {
    const auto tools = language == SourceLanguage::C
                           ? std::vector<std::string>{ crossTool( "gcc" ) }
                           : std::vector<std::string>{ crossTool( "as" ), crossTool( "ld" ) };
    for ( const std::string &tool : tools ) {
        if ( !isInPath( tool ) ) {
            return tool;
        }
    }
    if ( needsQemu && qemuProgram().empty() ) {
        return "qemu-aarch64";
    }
    return "";
}

void skipWhenMissing( SourceLanguage language, bool needsQemu ) {
    const std::string tool = missingTool( language, needsQemu );
    if ( !tool.empty() ) {
        GTEST_SKIP() << tool << " is not installed";
    }
}

} // namespace

void AssemblyTest::SetUp() {
    ProgramTest::SetUp();
    skipWhenMissing( SourceLanguage::Assembly, false );
}

void CTest::SetUp() {
    ProgramTest::SetUp();
    skipWhenMissing( SourceLanguage::C, false );
}

void AssemblyDebugTest::SetUp() {
    ProgramTest::SetUp();
    skipWhenMissing( SourceLanguage::Assembly, true );
}

void CDebugTest::SetUp() {
    ProgramTest::SetUp();
    skipWhenMissing( SourceLanguage::C, true );
}
