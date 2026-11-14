#pragma once

#include <span>
#include <string>
#include <vector>

extern const std::span<const char *const> aarch64Mnemonics;
extern const std::span<const char *const> aarch64Conditions;
extern const std::span<const char *const> aarch64Directives;
extern const std::span<const char *const> aarch64OperandKeywords;
extern const std::span<const char *const> aarch64SystemRegisters;

/// @return x0-x30, w0-w30, v/q/d/s/h/b0-31, sp, wsp, xzr, wzr, lr and fp.
const std::vector<std::string> &aarch64Registers();
