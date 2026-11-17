# Plan

The work agreed on 2026-11-15. The design it leads to is in
[ARCHITECTURE.md → Target architecture](ARCHITECTURE.md#target-architecture); this file is the
order of the work. Work goes one step at a time: plan the step, implement it, verify it, then
agree on the next one.

## Part 1: Restructuring

Each step keeps the IDE working. Outside behaviour does not change unless the step says so.

### 1. Faster build of the IDE

- FetchContent downloads commit archives (`https://github.com/<owner>/<repo>/archive/<hash>.tar.gz`
  with `URL_HASH SHA256=…`) instead of cloning whole git histories.
- `run.sh` installs `gcc-aarch64-linux-gnu` (and its equivalents for dnf, pacman, zypper), which
  C programs need.

Done when: a clean configure takes seconds instead of ~25 s, and C examples build on a fresh
machine after `./run.sh`.

### 2. Tests, clang-tidy, CI

- GoogleTest through FetchContent, a test target linked against `arm_ide_app`, run with
  `ctest`.
- Tests for the build (assembly and C, error messages with lines, the line table) and the
  debugger (breakpoints, continue from a breakpoint, line step in assembly and C, stepping over
  `svc`, leaving glibc for `main`, changing breakpoints of a running program).
- Tests skip with a reason when the cross tools or QEMU are missing.
- A `.clang-tidy` that checks the C++ guidelines (function size, naming, no globals where it
  can tell).
- A GitHub Actions workflow on every push: build, clang-tidy, `ctest`, with cached
  dependencies.

Done when: `ctest` passes locally and in Actions, and clang-tidy runs in both.

### 3. Text in `app`, `Session` in parts

- `app` owns the document text and version; the editor mirrors it (decision 1).
- `Session` is split into workspace, build, program, language services and diagnostics parts
  with private state and `const &` access; one event queue for one-time requests (decision 5).
- No globals: `Session` and `Terminal` get an owner, the editor callbacks stop using
  `drawingSession`/`drawingBuffer`.
- Completion, the `#include` popup rule and the declarations rule move from `gui/` to `app/`.

Done when: the tests pass, the IDE behaves as before, and `gui/` makes no decisions about
languages or completion.

### 4. Language tables, AArch64 in one place

- `Language` and `Isa` in `core`; tables per layer with a `static_assert` on their size
  (decision 3).
- Every `== SourceLanguage::C` comparison is replaced by a table lookup.
- AArch64 knowledge (tool prefixes, QEMU, registers, condition meanings, `svc`) is gathered in
  one place; condition meanings move from the Registers panel into `core`.

Done when: adding a language means adding enum values and table rows only.

### 5. Jobs instead of blocking calls

- A job runs a sequence of commands as child processes polled in `update()`, streams output
  to the log and can be cancelled (decision 4).
- The build, `objdump`, `nm` and the header declarations run as jobs; results carry the
  document version they were made for.
- The status bar shows a running build; Stop cancels it.

Done when: no tool runs inside a frame, and the window keeps drawing during a build.

### 6. One diagnostics model

- One diagnostic type for every source and every file (decision 6); the paused line leaves it.
- Errors through `std::expected` everywhere.
- gcc's JSON diagnostics and the strict `as`/`ld` format replace guessing with `std::regex`.

Done when: build errors in any file of the program show in the editor, and `std::regex` is
gone.

### 7. Token kinds for highlighting in `app`

- `app` tells for each line what each piece is; `gui` only maps kinds to colours (decision 7).

Done when: `gui/syntax.cpp` has no word lists or rules of its own.

### 8. AppImage release

- A GitHub Actions workflow on a tag builds an AppImage and attaches it to the release.

Done when: the AppImage from a release starts on a clean Ubuntu and on another distribution.

## Part 2: Features

After the restructuring, in this order unless agreed otherwise. Each one should be a table row
or a new part, not edits all over the tree.

### C and C++

1. **C++**: `.cpp`/`.cc`/`.hpp`, built with `g++`, highlighting.
2. **clangd** for C and C++: completion by context, errors while typing, signature in the
   popup. Replaces `core/header_declarations` and the hand-made C completion. clangd sees the
   AArch64 headers (`--target`, `--query-driver`, our `compile_commands.json`).
3. **Compiler choice**: gcc by default, clang by a click when installed.

### Assembly

4. **Errors while typing**: `as` runs in the background after a pause in typing.
5. **Completion by position**: only conditions after `b.`, only labels after `b`/`bl`/`adr`,
   only system registers after `mrs`/`msr`, registers of the right width, section names after
   `.section`.
6. **Document symbols**: labels (also `.global` ones of other open files), `.equ`/`.set`
   constants, macros and their parameters, local labels `1f`/`1b`, Linux system calls by name
   (numbers from the cross `unistd.h`).
7. **Ranking and fuzzy matching**: frequent instructions and names used in the file first.
8. **Operand forms and descriptions in the popup**: a table generated by assembling operand
   templates with `as`, short descriptions shared with hover help.
9. **Navigation**: go to a label, find its uses, rename it, list of labels.

### Programs and projects

10. **Playground**: autosaved single files, F5 without a save dialog, "Save as project".
11. **Folder as a project**: all sources in one program, output in `build/`, a clear error for
    two entry points, a file tree.
12. **CMake**, then **Make** (decision 2).
13. **Native ARM machine**: native gcc, no QEMU.

### Understanding the program

14. **Registers**: changed registers highlighted, flags explained, "this branch will be taken
    because Z=0".
15. **C ↔ assembly view**: the assembly of the C/C++ source next to it, lines linked by colour,
    `-O0`/`-O2`/`-O3` by a click, rebuilt after a pause in typing, gcc and clang side by side.
16. **System calls panel**: `write(1, "hi", 2) = 2` as the program runs (`qemu-aarch64
    -strace`).
17. **SIMD registers by lanes**: `v0` as 16×b, 8×h, 4×s, 2×d.
18. **Stack panel** and **call stack**, step over / step out.
19. **Instruction help**: hover text and the instruction's encoding by bit fields.
20. **Step back**: undo a step by recording registers and changed memory.
21. **Execution heat map**: how many times each line ran, from QEMU plugins.
22. **Calling convention checks while running**: callee-saved registers not restored, `sp` not
    aligned to 16.
23. **Editor basics**: find/replace, go to line.

### Later

- Running and debugging on a device over ssh + gdbserver.
- A TUI frontend.
- Other ISAs (x86-64, RISC-V, MIPS) as rows of the `Isa` table.
- Bare metal (`qemu-system`, microcontrollers).
