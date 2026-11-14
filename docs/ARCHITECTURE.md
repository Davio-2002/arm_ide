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
| `gdb_remote` | The GDB Remote Serial Protocol transport: packets, checksums, hex. No debugging logic. |
| `debugger` | A program under `qemu-aarch64 -g`: breakpoints, continue, line step, registers, memory. `PendingStop` says what happens at the next trap. |
| `terminal` | The user's shell on a pty, its screen kept by libvterm. |

### app/

| File | Job |
|---|---|
| `commands.h` | `Command` (every user command), `CommandInput` (what the frontend gives a command), `PathRequest`. |
| `session` | The one `Session` class: documents, commands, build, run, debug, console output, line marks, `PausedState`. |
| `documents` | The `Document` struct, names and display of paths, the config directory, recent files. |
| `completion` | Which words complete a typed term. |
| `aarch64_words` | Generated lists of every mnemonic, directive and register the assembler accepts. |

### gui/

| File | Job |
|---|---|
| `main.cpp` | The frame loop. |
| `window`, `settings`, `fonts`, `theme`, `color_schemes`, `icons` | Window, `imgui.ini`, fonts, colours, icons. |
| `commands` | Shortcuts table, menu items and buttons for commands, the request queue. |
| `menu`, `toolbar`, `statusbar` | The top bar and the bottom bar. |
| `dialogs`, `appearance` | Modal popups: file browser, discard changes, Appearance. |
| `syntax` | The editor's AArch64 and C language definitions. |
| `widgets` | Shared buttons and pill tabs. |
| `panels/` | One file per panel; `panels.cpp` lists them in one table. |

## One frame

```
waitForNextFrame( busy )    input, a window event, or the next frame is due
session.update()            program output, debugger stop replies
updateTerminalView()        shell output
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
  program are changed by sending SIGTRAP to QEMU (SIGINT would kill it).
- **Programs run on a pseudo-terminal**, so glibc flushes `printf` per line and input arrives
  in lines; Ctrl+D in the Console input sends end of file.
- **`Session`, the terminal and the dialogs keep fixed addresses**: requests, libvterm and
  dialog callbacks hold pointers to them.
- **Static initialization order**: the editor palette is set at runtime, and the scheme pointers
  in `theme.cpp` point into a constant-initialized table, because a value computed in another
  file's initializer may not be ready yet.
- **`as -g` segfaults when its working directory was deleted**; tools run in the source's
  folder.

## Build

```bash
cmake --preset release
cmake --build --preset release
./build/arm_ide examples/hello.s
clang-format -i src/*/*.cpp src/*/*.h src/gui/panels/*.cpp src/gui/panels/*.h
```
