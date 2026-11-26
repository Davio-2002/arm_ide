#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <test_support.h>

#include <app/session/session.h>

namespace {

const std::string exitProgram = ".global _start\n"      // 1
                                "_start: mov x0, #0\n"  // 2
                                "        mov x8, #93\n" // 3
                                "        svc #0\n";     // 4

/// @return The file's whole content.
std::string readFile( const std::string &path ) {
    std::stringstream content;
    content << std::ifstream( path ).rdbuf();
    return content.str();
}

/// @brief Keeps the session's recent files in the test's folder, not the user's config.
void useTestConfig( const std::filesystem::path &folder ) {
    setenv( "XDG_CONFIG_HOME", ( folder / "config" ).c_str(), 1 );
}

class DocumentTest : public ProgramTest {
  protected:
    void SetUp() override {
        ProgramTest::SetUp();
        useTestConfig( folder );
    }
};

class DocumentBuildTest : public AssemblyTest {
  protected:
    void SetUp() override {
        AssemblyTest::SetUp();
        useTestConfig( folder );
    }
};

} // namespace

TEST_F( DocumentTest, OpenedDocumentHasFileText ) {
    Session session;
    ASSERT_TRUE( session.openFile( writeSource( "exit.s", exitProgram ) ) );

    EXPECT_EQ( session.activeDocument().text, exitProgram );
    EXPECT_FALSE( isModified( session.activeDocument() ) );
}

TEST_F( DocumentTest, EditMakesDocumentModifiedWithNewVersion ) {
    Session session;
    ASSERT_TRUE( session.openFile( writeSource( "exit.s", exitProgram ) ) );
    const Document &document = session.activeDocument();
    const size_t openedVersion = document.version;

    session.editDocument( document.id, exitProgram + "// end\n" );

    EXPECT_TRUE( isModified( session.activeDocument() ) );
    EXPECT_GT( session.activeDocument().version, openedVersion );
}

TEST_F( DocumentTest, SameTextKeepsVersion ) {
    Session session;
    ASSERT_TRUE( session.openFile( writeSource( "exit.s", exitProgram ) ) );
    const size_t openedVersion = session.activeDocument().version;

    session.editDocument( session.activeDocument().id, exitProgram );

    EXPECT_EQ( session.activeDocument().version, openedVersion );
}

TEST_F( DocumentTest, ReturningToSavedTextIsNotModified ) {
    Session session;
    ASSERT_TRUE( session.openFile( writeSource( "exit.s", exitProgram ) ) );
    const int id = session.activeDocument().id;

    session.editDocument( id, exitProgram + "// end\n" );
    session.editDocument( id, exitProgram );

    EXPECT_FALSE( isModified( session.activeDocument() ) );
}

TEST_F( DocumentTest, SaveWritesDocumentText ) {
    Session session;
    const std::string path = writeSource( "exit.s", exitProgram );
    ASSERT_TRUE( session.openFile( path ) );
    const std::string edited = exitProgram + "// end\n";

    session.editDocument( session.activeDocument().id, edited );
    session.run( Command::Save, {} );

    EXPECT_EQ( readFile( path ), edited );
    EXPECT_FALSE( isModified( session.activeDocument() ) );
}

TEST_F( DocumentTest, SaveAsWritesUntitledDocumentText ) {
    Session session;
    const std::string path = ( folder / "new.s" ).string();

    session.editDocument( session.activeDocument().id, exitProgram );
    CommandInput input;
    input.path = path;
    session.run( Command::SaveAs, input );

    EXPECT_EQ( readFile( path ), exitProgram );
    EXPECT_EQ( session.activeDocument().path, path );
    EXPECT_FALSE( isModified( session.activeDocument() ) );
}

TEST_F( DocumentBuildTest, BuildUsesEditedText ) {
    Session session;
    ASSERT_TRUE( session.openFile( writeSource( "exit.s", exitProgram ) ) );
    const std::string broken = ".global _start\n"       // 1
                               "_start: mov x0, #0\n"   // 2
                               "        movv x8, #93\n" // 3
                               "        svc #0\n";      // 4

    session.editDocument( session.activeDocument().id, broken );
    session.run( Command::Build, {} );

    EXPECT_FALSE( session.buildSucceeded );
    ASSERT_EQ( session.lineMarks.size(), 1u );
    EXPECT_EQ( session.lineMarks.front().line, 3 );
}
