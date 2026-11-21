#include <test_support.h>

#include <core/process.h>

namespace {

const int stopTimeoutMilliseconds = 5000;

const std::string systemCallProgram = ".global _start\n"       // 1
                                      "_start: mov x8, #172\n" // 2 getpid
                                      "        svc #0\n"       // 3
                                      "        mov x0, #3\n"   // 4
                                      "        mov x8, #93\n"  // 5 exit
                                      "        svc #0\n";      // 6

const std::string callProgram = "int square( int x ) {\n"    // 1
                                "    return x * x;\n"        // 2
                                "}\n"                        // 3
                                "\n"                         // 4
                                "int main( void ) {\n"       // 5
                                "    int a = 2;\n"           // 6
                                "    int b = square( a );\n" // 7
                                "    return b - 4;\n"        // 8
                                "}\n";                       // 9

const std::string inputProgram = "#include <stdio.h>\n"       // 1
                                 "\n"                         // 2
                                 "int main( void ) {\n"       // 3
                                 "    int n = 0;\n"           // 4
                                 "    scanf( \"%d\", &n );\n" // 5
                                 "    return n;\n"            // 6
                                 "}\n";                       // 7

const std::string loopProgram = "int main( void ) {\n"       // 1
                                "    volatile long i = 0;\n" // 2
                                "    while ( i >= 0 ) {\n"   // 3
                                "        i++;\n"             // 4
                                "    }\n"                    // 5
                                "    return 0;\n"            // 6
                                "}\n";                       // 7

const std::string sumProgram = "int square( int x ) {\n"                   // 1
                               "    return x * x;\n"                       // 2
                               "}\n"                                       // 3
                               "\n"                                        // 4
                               "int main( void ) {\n"                      // 5
                               "    int s = 0;\n"                          // 6
                               "    for ( int i = 0; i < 3; i++ ) {\n"     // 7
                               "        s += square( i ) + square( s );\n" // 8
                               "    }\n"                                   // 9
                               "    return s;\n"                           // 10
                               "}\n";                                      // 11

/// @brief Builds the source and starts it under the debugger, paused at its entry point.
/// @return Empty on success, otherwise what went wrong.
std::string startProgram( Debugger &debugger, const std::string &source ) {
    const BuildResult build = buildProgram( source );
    if ( !build.success ) {
        return "build failed";
    }
    if ( !startDebugger( debugger, build.executablePath, source ) ) {
        return debugger.error;
    }
    return "";
}

/// @return The source line of the PC.
int pausedLine( Debugger &debugger ) {
    return sourceLineAt( debugger, readRegisters( debugger ).pc );
}

} // namespace

TEST_F( AssemblyDebugTest, StopsAtBreakpoint ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.s", systemCallProgram ) ), "" );

    setBreakpointLines( debugger, { 4 } );
    runDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 4 );
}

TEST_F( AssemblyDebugTest, StopsAtBreakpointOnFirstInstruction ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.s", systemCallProgram ) ), "" );

    setBreakpointLines( debugger, { 2 } );
    runDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 2 );
}

TEST_F( AssemblyDebugTest, ContinuesFromBreakpoint ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.s", systemCallProgram ) ), "" );
    setBreakpointLines( debugger, { 2, 4 } );
    runDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );
    ASSERT_EQ( pausedLine( debugger ), 2 );

    continueDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );
    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 4 );

    continueDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );
    EXPECT_EQ( debugger.state, DebuggerState::Exited );
    EXPECT_EQ( debugger.exitCode, 3 );
}

TEST_F( CDebugTest, ContinueStopsOncePerRunOfALine ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "sum.c", sumProgram ) ), "" );
    setBreakpointLines( debugger, { 8 } );
    runDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );

    int stops = 0;
    while ( debugger.state == DebuggerState::Paused && stops < 10 ) {
        EXPECT_EQ( pausedLine( debugger ), 8 );
        stops++;
        continueDebugger( debugger );
        pollDebugger( debugger, stopTimeoutMilliseconds );
    }

    EXPECT_EQ( stops, 3 );
    EXPECT_EQ( debugger.state, DebuggerState::Exited );
    EXPECT_EQ( debugger.exitCode, 6 );
}

TEST_F( AssemblyDebugTest, LineStepRunsOneInstruction ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.s", systemCallProgram ) ), "" );

    stepDebugger( debugger );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 3 );
}

TEST_F( AssemblyDebugTest, LineStepOverSystemCallStopsAfterIt ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.s", systemCallProgram ) ), "" );
    stepDebugger( debugger );
    ASSERT_EQ( pausedLine( debugger ), 3 );

    stepDebugger( debugger );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 4 );
}

TEST_F( CDebugTest, LineStepFromStartupStopsAtMain ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.c", callProgram ) ), "" );
    ASSERT_EQ( pausedLine( debugger ), 0 );

    stepDebugger( debugger );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( readRegisters( debugger ).pc, debugger.mainAddress );
    EXPECT_EQ( pausedLine( debugger ), 5 );
}

TEST_F( CDebugTest, LineStepGoesToNextLine ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "calls.c", callProgram ) ), "" );
    setBreakpointLines( debugger, { 6 } );
    runDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );
    ASSERT_EQ( pausedLine( debugger ), 6 );

    stepDebugger( debugger );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 7 );
}

TEST_F( CDebugTest, SetsBreakpointsWhileRunning ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "loop.c", loopProgram ) ), "" );
    runDebugger( debugger );
    pollDebugger( debugger, 200 );
    ASSERT_EQ( debugger.state, DebuggerState::Running );

    setBreakpointLines( debugger, { 4 } );
    pollDebugger( debugger, stopTimeoutMilliseconds );

    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 4 );
}

TEST_F( CDebugTest, KeepsWaitingForInputWhenBreakpointsChange ) {
    ASSERT_EQ( startProgram( debugger, writeSource( "input.c", inputProgram ) ), "" );
    runDebugger( debugger );
    pollDebugger( debugger, 200 );
    ASSERT_EQ( debugger.state, DebuggerState::Running ) << "the program should wait for input";

    setBreakpointLines( debugger, { 6 } );
    pollDebugger( debugger, 200 );
    ASSERT_EQ( debugger.state, DebuggerState::Running ) << "the program should still wait";
    writeToProcess( debugger.qemu, "7\n" );
    pollDebugger( debugger, stopTimeoutMilliseconds );
    ASSERT_EQ( debugger.state, DebuggerState::Paused );
    EXPECT_EQ( pausedLine( debugger ), 6 );

    continueDebugger( debugger );
    pollDebugger( debugger, stopTimeoutMilliseconds );
    EXPECT_EQ( debugger.state, DebuggerState::Exited );
    EXPECT_EQ( debugger.exitCode, 7 );
}
