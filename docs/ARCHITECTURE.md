# Architecture

ARM IDE is a small IDE for AArch64 assembly and C. It builds with the GNU cross tools, runs
programs in `qemu-aarch64`, and debugs them through QEMU's GDB stub. Linux only.

## Layers

```
src/core/   talks to the system: processes, the cross tools, QEMU's GDB stub, a shell on a pty
src/app/    decides: documents, commands, build/run/debug, what the user sees as data
src/gui/    draws: the Dear ImGui window, panels, themes, fonts, the code editor
```

Dependencies point down only: `gui → app → core`. Each folder is a CMake target
(`arm_ide_core`, `arm_ide_app`, the `arm_ide` executable). The two libraries do not link ImGui,
GLFW or the editor, so a GUI include in `core/` or `app/` does not build. A second frontend (a
TUI, for example) would be a new folder next to `gui/` that uses `app/` the same way.

To decide where code goes, ask: *would another frontend need this?* If yes, it is not in `gui/`.

### core/

| File | Job |
|---|---|
| `process` | Start, poll, write to and kill child processes; run a command to the end. |
| `toolchain` | Find the cross tools, build a source (`.s`: `as` + `ld`, `.c`: `gcc -static`), parse their messages, read the DWARF line table and symbols. |
| `header_declarations` | The functions and macros a C file's headers declare, from `gcc -aux-info` and `gcc -E -dM` over its preprocessor lines, and the compiler's `#include <...>` folders (for autocomplete). |
| `gdb_remote` | The GDB Remote Serial Protocol transport: packets, checksums, hex. No debugging logic. |
| `debugger` | A program under `qemu-aarch64 -g`: breakpoints, continue, line step, registers, memory. `PendingStop` says what happens at the next trap. |
| `terminal` | The user's shell on a pty, its screen kept by libvterm. |

### app/

| File | Job |
|---|---|
| `commands.h` | `Command` (every user command), `CommandInput` (what the frontend gives a command), `PathRequest`. |
| `session` | The one `Session` class: documents, commands, build, run, debug, console output, line marks, `PausedState`. |
| `documents` | The `Document` struct, names and display of paths, the config directory, recent files. |
| `completion` | Which words complete a typed term, per language: `completeAssembly`, `completeC` (C also gets the document's header declarations, read at open, save and when an edit changed the preprocessor lines), `completeDirective` for the word after a line's leading `#` (typing `#` starts autocomplete in C), and `completeInclude` for the header name of an `#include` line. |
| `aarch64_words` | Generated lists of every mnemonic, directive and register the assembler accepts. |

### gui/

| File | Job |
|---|---|
| `main.cpp` | The frame loop; owns the `Session` and the `TerminalView`. |
| `window`, `settings`, `fonts`, `theme`, `color_schemes`, `icons` | Window, `imgui.ini`, fonts, colours, icons. |
| `commands` | Shortcuts table, menu items and buttons for commands, the request queue. |
| `menu`, `toolbar`, `statusbar` | The top bar and the bottom bar. |
| `dialogs`, `appearance` | Modal popups: file browser, discard changes, Appearance. |
| `syntax` | The editor's AArch64 and C language definitions. C colours `#include`/`#define` words, the header name after `#include` (scheme colour `headerName`, palette slot `customColor1`) and strings itself (`tokenizeC`), instead of one colour for the whole preprocessor line. |
| `widgets` | Shared buttons and pill tabs. |
| `panels/` | One file per panel; `panels.cpp` lists them in one table. |

## One frame

```
waitForNextFrame( busy )    input, a window event, or the next frame is due
session.update()            program output, debugger stop replies
updateTerminalView( view )  shell output
drawMenuBar / drawStatusBar / drawPanels
    syncEditorWithSession   a text buffer per document, edit versions reported, line marks
    every panel draws       reads the Session, never changes it; user actions are *requested*
runRequests( session )      the requested commands and actions run, in order
drawDialogs()               a dialog's answer runs its callback
```

Drawing gets a `const Session &`. Everything that changes the session goes through
`requestCommand` or `requestAction` (`gui/commands.h`), so no panel sees a half-changed session
and every change happens in one place. Requests are dropped while a dialog is open.

`waitForNextFrame` (`gui/window.cpp`) keeps an idle IDE asleep. While a program, the debugger
or the terminal is busy, and for 0.5 s after input, frames run at the display rate. Otherwise it
waits up to 0.4 s (the caret blink, shell output). Under Wayland the window's own protocol
traffic ends a GLFW wait at once, so the wait loops until the deadline unless ImGui got input or
a resize, redraw, close or drop arrived.

## Documents and text

`Session` owns the list of documents (path, the text as on disk, edit version, saved version)
and which one is active. The frontend owns the text being edited: one `TextEditor` per document
id in `gui/panels/editor.cpp`. Each frame the editor reports each buffer's undo index as the
document's version, so `isModified()` is decided in `app/`. A command that saves gets the
active text in its `CommandInput`.

## Recipes

### Add a command (for example *Step Out*)

1. `app/commands.h`: add `Command::StepOut`.
2. `app/session.cpp`: add it to the switches of `commandLabel`, `canRun`, `pathNeeded` (if it
   needs a file) and `run`. Put the work in a `Session` method; CPU or protocol work goes in
   `core/debugger`.
3. `gui/commands.cpp`: add its shortcut to `shortcuts[]`, if it has one.
4. `gui/menu.cpp`: `commandMenuItem( session, Command::StepOut )` in its menu; optionally
   `commandButton(...)` in `gui/toolbar.cpp` or a panel header.

### Add a panel (for example *Stack*)

1. `gui/panels/stack.cpp` with `void drawStackPanel( const Session &session )`, starting with
   `beginPanel( "Stack" )` and ending with `ImGui::End()`. Declare it in `gui/panels/panels.h`.
2. A row in `panels[]` in `gui/panels/panels.cpp`: name, dock area, whether the tool strip can
   hide it, icon, draw function.
3. Add the file to `arm_ide` in `CMakeLists.txt`.
4. To have it in the default layout for existing users, bump `"MainDockSpace v3"` in
   `panels.cpp`; otherwise it opens floating once and the user docks it.

### Show more of the paused program (for example the call stack)

1. Read it in `core/debugger` (a new function using `gdbRequest`).
2. Add a field to `PausedState` in `app/session.h` and fill it in `Session::showPausedLine()`.
3. Draw it in a panel from `session.paused`.

### Add a setting

GUI-only settings (look, layout) are lines in the `[ArmIde][Settings]` section:
`readSettingsLine` and `writeSettings` in `gui/settings.cpp`. Call `ImGui::MarkIniSettingsDirty()`
when it changes. Settings another frontend needs belong in `app/`, in their own file under
`configDirectory()`, like `recent_files`.

### Add a colour

Add the field to `ColorScheme` in `gui/theme.h`, give it a value in every scheme in
`gui/color_schemes.cpp`, and read it with `currentScheme().yourColor`.

### Add an icon

Put `name.svg` in `assets/icons/` (CMake turns every SVG there into `name_svg.inc`). Add a value
to `Icon` in `gui/icons.h`, and in `gui/icons.cpp` an array including `name_svg.inc` and a row in
`iconSources[]`. List the icon in `assets/icons/NOTICE.md`.

## Things that are not obvious

- **QEMU continues into a breakpoint at once.** Continuing from a PC with a breakpoint first
  steps one instruction (`PendingStop::StepOverBreakpoint`), then continues.
- **QEMU's single step over `svc` runs one instruction too many**, so a step over `svc` runs to
  a temporary breakpoint at PC+4 instead.
- **QEMU ignores the `0x03` interrupt byte** while the program runs. Breakpoints of a running
  program are changed by sending SIGTRAP to QEMU (SIGINT would kill it). QEMU returns EINTR
  to a system call the signal interrupts (a `read` waiting for input), so QEMU is first held
  with SIGSTOP, the call's first argument read from `/proc/<pid>/syscall`, and after the stop
  `x0` and the PC are set back so the `svc` runs again.
- **A line's breakpoint is at its first address only.** A C line has several rows in the
  line table (one after each call), and a breakpoint on each made Continue stop on the same
  line again.
- **Programs run on a pseudo-terminal**, so glibc flushes `printf` per line and input arrives
  in lines; Ctrl+D in the Console input sends end of file.
- **`Session`, the terminal and the dialogs keep fixed addresses**: requests, libvterm and
  dialog callbacks hold pointers to them.
- **Static initialization order**: the editor palette is set at runtime, and the scheme pointers
  in `theme.cpp` point into a constant-initialized table, because a value computed in another
  file's initializer may not be ready yet.
- **`as -g` segfaults when its working directory was deleted**; tools run in the source's
  folder.

## Target architecture

Decided on 2026-11-15. The sections above describe the code as it is; this one describes where
it is going. Until the restructuring below is done, new code follows this section where the two
differ.

### Goal and scope

An IDE with full support of AArch64 assembly and C/C++ for comfortable development, research
and learning, open to new languages and instruction set architectures (ISAs) later.

- Linux programs for AArch64: cross-built on x86 and run in QEMU, or built natively on an ARM
  machine and run directly.
- Where a program runs is a choice: QEMU now, a device over ssh + gdbserver later.
- Bare metal (microcontrollers) is out of scope for now.
- The user never writes configuration. Behaviour follows what is open; a rare choice is a
  click, remembered by the IDE.

### Decisions

1. **Document text.** `app` owns the logical document: text, version, saved text, language and
   everything derived from it. The frontend keeps an editing mirror: cursor, selection, scroll,
   undo, rendering.
   - The frontend sends the whole text to `app` after every edit; consumers (clangd, the `as`
     check) wait for a pause themselves. Measure on a 10 000-line file; send edits instead of
     the whole text only if that is too slow.
   - The version is one counter per document, not a history. Whoever changes the text raises
     it; the frontend applies `app`'s text when its own version is behind.
   - A document is modified while its text differs from the saved text.
   - The cursor stays in the frontend and is passed with each request that needs it.
   - "Read-only while debugging" is decided by `app`; the frontend only applies it.
2. **What a program is.** One concept inside: a program is a set of files plus a target. How the
   set is chosen depends on what is open:
   - *Playground*: one file, saved automatically in a service folder, F5 without a save dialog,
     "Save as project" turns it into a folder.
   - *File from disk*: a program of that one file, where it lies.
   - *Folder without a build system*: every `.s`, `.c`, `.cpp` in it and its subfolders except
     `build/` and hidden ones. Built with `g++` if there is C++, `gcc` if there is C, otherwise
     `as` + `ld` without the C library. Output in `build/`.
   - *Folder with `CMakeLists.txt`*: CMake, with a generated AArch64 toolchain file, its own
     build folder, the Debug build type, executables from the CMake File API and its
     `compile_commands.json`.
   - *Folder with a `Makefile`*: `make CC=… CXX=…`; the user picks the executable once.
   - CMake wins over a Makefile. "Run this file alone" works inside a project.
   - The build methods are a table (detect, build, executables, compile commands); the build,
     the debugger and clangd don't know which one made the program. For our own builds we
     write `compile_commands.json` for clangd. QEMU gets the sysroot (`-L`) for dynamically
     linked programs. Flags are `-g -O0` for now, the optimization level becomes a click later.
     Every build rebuilds everything.
3. **Language and ISA knowledge.** Two names in `core`: `Language` (assembly, C, C++) and `Isa`
   (AArch64). Assembly is always `Language::Assembly` plus an `Isa`.
   - Each layer has its own table of plain structs indexed by them: `core` for building and
     tools, `app` for editing behaviour, `gui` for drawing. No single descriptor, no
     data-file language packs.
   - A `static_assert` checks that every table has a row for every value, so a forgotten row
     does not build.
   - Knowledge goes where it is used: facts about the ISA (what `hi` means in flags) live in
     `core`, panels only show them.
   - AArch64 knowledge is gathered in one place now; the `Isa` table gets rows with a second
     ISA.
   - Derive from tools instead of writing lists by hand: registers from QEMU's `target.xml`,
     system call numbers from the cross `unistd.h`, assembly words from `as`.
4. **Background work.** One thread. All background work is child processes polled in
   `update()`.
   - A worker thread only if a measurement asks for it, and only for a pure input-to-output
     function without access to `Session`.
   - A job is a sequence of commands whose output reaches the log line by line, and it can be
     cancelled.
   - A job remembers the document version it ran for; a newer check cancels an older one and
     results for an old version are dropped.
   - The status bar shows a running build, Stop cancels builds too, editing continues during
     a build.
   - Small blocking calls in `gui` (`fc-list`, `fc-match`, `gdbus`) stay unless measured as
     noticeable.
5. **Session.** Still the one class, but it only coordinates the rules between its parts (save
   before a build, read-only while debugging, breakpoints to the debugger after a build).
   - Parts are plain structs with free functions, each in its own `app/` file: *workspace*
     (documents, open folder or playground, recent files), *build* (build methods, jobs, build
     log), *program* (run, debugger, console, breakpoints, paused state, memory), *language
     services* (clangd, completion, the `as` check), *diagnostics*.
   - The frontend reads state every frame. Versions only for what the frontend mirrors (text,
     marks); one event queue for one-time requests (reveal a line, switch a tab), replacing
     `buildCount`/`buildSucceeded` and `takeLineToReveal()`.
   - Fields are private; the frontend gets `const &` to the parts' state and changes things
     only through `Session` methods.
6. **Diagnostics.** One type for every source (build, clangd, the `as` check while typing,
   checks while the program runs): file, line, column, error or warning, text.
   - Diagnostics of every file, not only the built one. The paused line is program state, not
     a diagnostic.
   - Editing a file removes its old build diagnostics; the checks while typing bring the
     correct ones back.
   - When a check while typing and the build report the same problem, the editor shows the
     first; the build message stays in the Build log.
   - Tool output is parsed without guessing: gcc's JSON diagnostics, the strict
     `file:line: Error:` format of `as` and `ld`.
7. **Highlighting.** `app` says what each piece of a line is (label, instruction, register,
   number, comment, …); any frontend uses it. Colours stay in the frontend. Highlighting by
   meaning from clangd comes later as the same kinds.
8. **Verification.** Automatic tests with GoogleTest, only for the debugger and the build (the
   fragile parts during the restructuring). They run through `core` and `app` without a
   window, with `ctest`, and are skipped with a reason when the cross tools or QEMU are
   missing.
9. **Compilers.** Assembly is always GNU `as`. The C/C++ compiler is a choice in the language
   table: gcc by default, clang by a click when installed (clang still needs the GNU cross
   C library and a linker). Later the C ↔ assembly view can compare gcc and clang.
10. **Build speed and releases.** Measured on 2026-11-15 (16 cores): fetching dependencies 25 s
    (full git histories, 161 MB), compiling 13 s; the slowest own file is `core/toolchain.cpp`
    (9.6 s, `std::regex`).
    - FetchContent downloads commit archives with a SHA256 instead of cloning.
    - `std::regex` goes away when tool output parsing is redone (decision 6).
    - GitHub Actions: a check on every push (build, clang-tidy, `ctest`, cached dependencies)
      and a release on a tag.
    - Releases are an AppImage. The cross tools and QEMU still come from the user's
      distribution.

### C++ guidelines

Binding for all code. C++23 (the project already uses it), errors through `std::expected`.

**Code**
- Simple and readable for an average developer. No tricks.
- Modern idioms only when they make code shorter and clearer.
- A function does one thing, at most 40 lines and 4 parameters.
- Early return instead of nesting. Depth at most 3.
- No double negatives (`!isNotEmpty`).
- `auto` only when the type is obvious.

**Names**
- A name says what the thing does.
- Functions are verbs, types are nouns, bools start with `is`/`has`/`can`.
- Needing "And" in a name means the function should be split.
- Name length grows with scope.

**Comments**
- Public API: Doxygen, `@brief` in one sentence.
- `@param`/`@return` only when not obvious.
- Inside functions, only *why*, never *what*.
- Don't document the trivial.

**Resources and types**
- RAII. No bare `new`/`delete`.
- `unique_ptr` by default; `shared_ptr` only for shared ownership.
- A raw pointer or reference does not own.
- `const` wherever possible. Heavy things by `const &`.
- `enum class` and `constexpr` instead of macros.
- One error policy: `std::expected`, never mixed with exceptions or `bool` plus a status
  string.

**Architecture**
- Layers: UI → logic → platform. Dependencies point down only.
- A lower layer calls an upper one through a callback or an event.
- Composition over inheritance. Inheritance at most 2 levels deep.
- An interface only with at least 2 implementations or for tests.
- No globals or singletons. Dependencies come through the constructor.
- Data without an invariant is a `struct` with free functions.
- A module is a folder: a small public header, details hidden.
- No circular dependencies. What is shared goes into its own module.
- Templates against duplication, constrained with concepts.
- One owner per resource. Ownership is a tree.
- Complexity matches the current task. Nothing "for the future".

**Checks**
- clang-format and clang-tidy are mandatory.
- A newcomer understands the structure in 10 minutes.

Known violations to remove during the restructuring: the mix of `bool`, status strings and
`std::optional` for errors.

### Restructuring order

The steps of the restructuring and the features after it are in [PLAN.md](PLAN.md).

## Build

```bash
cmake --preset release
cmake --build --preset release
./build/arm_ide examples/hello.s
clang-format -i src/*/*.cpp src/*/*.h src/gui/panels/*.cpp src/gui/panels/*.h
```

Tests and clang-tidy, as the Check workflow runs them on every push:

```bash
cmake --preset dev && cmake --build --preset dev
ctest --preset dev
run-clang-tidy -p build-dev -quiet "$PWD/(src|tests)/"
```

## Release

`packaging/appimage.sh` builds `build/appimage/ARM_IDE-<version>-x86_64.AppImage`: a release
build installed into an AppDir (`cmake --install`: the program, `.desktop` file, icons), packed
by a pinned linuxdeploy. Bundled libraries are found through the program's `RUNPATH`, not
`LD_LIBRARY_PATH`, so the shell, the cross tools and QEMU that the IDE starts use the system's
libraries. The cross tools, QEMU, `fc-list`/`fc-match` and `gdbus` are not bundled.

To release, raise `VERSION` in `project()` of `CMakeLists.txt`, merge to `main`, then push the
matching tag; the Release workflow checks that the tag matches, builds the AppImage on Ubuntu
24.04 (so glibc 2.39 or newer is needed) and attaches it to a GitHub release:

```bash
git tag v0.1.0
git push origin v0.1.0
```
