FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.tar.gz
    URL_HASH SHA256=745c55415660044610f7fcd3af7a6420d5de16a7dbb9ebfe2e131275676232be
    SYSTEM)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(arm_ide_tests
    test_support.cpp
    build_tests.cpp)
target_link_libraries(arm_ide_tests PRIVATE arm_ide_app GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(arm_ide_tests)
