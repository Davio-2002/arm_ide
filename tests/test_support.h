#pragma once

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

#include <core/debugger.h>
#include <core/toolchain.h>

/// @brief A test with its own temporary folder, since a build writes next to its source.
class ProgramTest : public testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;

    /// @brief Writes a source file into the test's folder.
    /// @return Absolute path of the file.
    std::string writeSource( const std::string &name, const std::string &text ) const;

    std::filesystem::path folder;
};

/// @brief Skips when the assembler or the linker is missing.
class AssemblyTest : public ProgramTest {
  protected:
    void SetUp() override;
};

/// @brief Skips when the C cross compiler is missing.
class CTest : public ProgramTest {
  protected:
    void SetUp() override;
};

/// @brief Skips when the assembler, the linker or QEMU is missing; stops QEMU at the end.
class AssemblyDebugTest : public ProgramTest {
  protected:
    void SetUp() override;
    void TearDown() override;

    Debugger debugger;
};

/// @brief Skips when the C cross compiler or QEMU is missing; stops QEMU at the end.
class CDebugTest : public ProgramTest {
  protected:
    void SetUp() override;
    void TearDown() override;

    Debugger debugger;
};
