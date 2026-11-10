# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A native mini IDE for AArch64 assembly: C++23, Dear ImGui (docking branch) on GLFW/OpenGL 3.3, the ImGuiColorTextEdit editor (goossens fork), real GNU cross tools for building, QEMU for running, and Unicorn for the built-in step debugger. Linux only (fork/exec, pipes).

## Commands

System packages: `binutils-aarch64-linux-gnu qemu-user libglfw3-dev libunicorn-dev libfreetype-dev`. ImGui (with its FreeType font loader, light hinting) and the editor are pulled by CMake `FetchContent`, pinned to commit hashes in `CMakeLists.txt`.

```bash
cmake --preset release            # configure (Ninja, clang++, Release, build/)
cmake --build --preset release    # build
./build/arm_ide examples/hello.s  # run; with no argument it reopens the most recent file
clang-format -i src/*.cpp src/*/*.cpp src/*/*.h
```

There is no test suite. Verify behaviour by running the app; logic in `core/` and `app/` can be exercised from a small standalone program linked against `build/libarm_ide_core.a` (no window needed).

## Architecture

Three layers under `src/`, dependencies only point downward:

- `core/` — no ImGui/GLFW. `process` (spawn/poll/kill child processes, blocking `runCommand`), `toolchain` (`as -g` + `ld`, parses tool output into typed `Message`s with source lines, reads the address→line table via `objdump --dwarf=decodedline`), `emulator` (Unicorn: loads ELF `PT_LOAD` segments, maps a stack, handles `svc` for write/read/exit, breakpoints via a `UC_HOOK_CODE` hook, run in time slices or single-step).
- `app/session` — the one `Session` class: open/save, build, run under QEMU, debug, breakpoints, register snapshots. It never draws; it exposes state as data (`console` of `Message`s with a `MessageKind`, `lineMarks` with a `LineMarkKind`, `pausedLine`, `lineToReveal`, `status`). `core/` and `app/` compile into the `arm_ide_core` library, which does not link ImGui, so GUI includes there fail to build.
- `gui/` — everything visible. `window` (GLFW, fonts, zoom, theme, settings file), `dialogs` (in-app modal popups: file browser for Open/Save As, discard-changes question), `menu` (shortcuts → `MenuActions` → `runActions` calling `Session`), `panels` (dock layout, editor + gutter + autocomplete, console, registers, memory; all colours live here), `aarch64_language` (syntax definition), `aarch64_words` (word lists it and autocomplete use).

Data flow per frame (`main.cpp`): `session.update()` polls the QEMU process and runs one 10 ms emulator slice → `drawMenuBar` handles input → `drawPanels` syncs the editor from `Session` (read-only while debugging, markers rebuilt only when `lineMarksVersion` changes, cursor moved to `lineToReveal`) and draws.

Non-obvious points:

- The editor's text and "modified" state (undo index) live in `gui/panels.cpp`; `Session` only saves the text it is given. File actions that touch both are in `gui/menu.cpp` (`openDocument`, `saveDocument`).
- `Session` must have a fixed address: Unicorn hooks receive `&emulator` as user data, and the settings handler and gutter callback keep a `Session*`. It is a single static in `main.cpp`.
- Breakpoint resume semantics: `continueEmulator`/`stepEmulator` set `skipBreakpointAt` to the current PC so resuming from a breakpoint doesn't stop immediately; starting fresh with F8 sets `state = Running` directly so a breakpoint on the first instruction still hits.
- Autocomplete is the editor's own popup (`SetAutoCompleteConfig`, Ctrl+Space or typing); `suggestCompletions` in `panels.cpp` only supplies words: the language's mnemonics, directives and registers plus identifiers in the document, prefix-matched ignoring case. `.` is a word character so `.glo` and `b.e` complete whole tokens.
- `aarch64_words.cpp` is generated data: every mnemonic, directive, `dc`/`ic`/`at`/`tlbi` operation and system register name that `aarch64-linux-gnu-as` (2.46, default `-march`, i.e. ARMv8-A + FP + AdvSIMD, no LSE/SVE/SME) accepts, found by assembling candidate names taken from `libopcodes`/`as` strings. Extension-only instructions are left out on purpose because the IDE's build would reject them.
- Set the editor palette at runtime (`setupEditor`). The `TextEditor` global's constructor copies a static palette from another translation unit, and static-init order made all colours transparent once.
- Dialogs are ImGui modals, not external programs: on GNOME Wayland a child process's window opens behind ours (no activation token), which looked like a freeze. They are asynchronous: `askForFileToOpen`/`askForFileToSave`/`askToDiscardChanges` take a callback that `drawDialogs` runs on a later frame, `whenChangesDiscarded` (menu) wraps the unsaved-changes check, and menu actions, dropped files and window close are ignored while `isDialogOpen()`.
- Settings (dock layout, zoom, recent files) are one ImGui ini at `$XDG_CONFIG_HOME/arm_ide/imgui.ini` (default `~/.config`), via a custom `ImGuiSettingsHandler` in `gui/window.cpp`. The default dock layout is built only when the dockspace ID `"MainDockSpace v2"` has no saved node; bump that string when adding a panel to the default layout.
- Tools run with the source's folder as working directory (`runCommand`'s `directory`), and `buildProgram` makes the source path absolute first. `as -g` (binutils 2.46) segfaults when its working directory has been deleted, which happens when the IDE was started inside a `build/` that was later recreated.
- Building `foo.s` writes `foo.o` and `foo` next to the source; a source without an extension is rejected so the executable can't overwrite it.

## Code style

- clang-format with the repo `.clang-format` (LLVM base, 4-space indent, 100 columns, `InsertBraces`, spaces inside parentheses: `if ( x )`, `f( a, b )`, empty `f()`). Run it on every changed file.
- Every `if`/loop body in braces; no comments that restate the code.
- Keep it simple: free functions and plain structs, one `Session` class; no extra interfaces, wrappers or config for fixed values.
- New behaviour goes in the right layer: CPU/tool logic in `core/`, IDE behaviour in `Session`, drawing and colours in `gui/`. A new command is a `MenuActions` field, a shortcut in `readShortcuts`, a menu item in `drawMenus`, and a call in `runActions`.
- The owner prefers working step by step: propose a plan, implement one step, verify, then wait.
