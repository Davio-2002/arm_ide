#pragma once

#include <span>

// Every word here is accepted by aarch64-linux-gnu-as 2.46 with its default -march
// (ARMv8-A with FP and AdvSIMD), the assembler the IDE builds with.
extern const std::span<const char *const> aarch64Mnemonics;
extern const std::span<const char *const> aarch64Conditions;
extern const std::span<const char *const> aarch64Directives;
extern const std::span<const char *const> aarch64OperandKeywords;
extern const std::span<const char *const> aarch64SystemRegisters;
