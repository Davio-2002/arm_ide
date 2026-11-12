# ARM IDE

A small desktop IDE for writing, running and debugging **AArch64 (ARM64) assembly** on a regular x86 Linux PC.

You write a `.s` file, press **F5**, and see the output. You can also step through it instruction by instruction while watching the registers and memory.

## Quick start

On Ubuntu or Debian:

```bash
git clone https://github.com/Davio-2002/arm_ide.git
cd arm_ide
./run.sh examples/hello.s
```

On the first run, `run.sh` installs any missing packages (it asks for your `sudo` password), downloads two libraries and builds the IDE. This takes a minute or two. Later runs start right away.

To start the IDE without a file:

```bash
./run.sh
```

It reopens the file you had open last time.

## Using it

| Key | Action |
|---|---|
| **F5** | Build and run |
| Shift+F5 | Stop |
| F7 | Build only |
| **F9** | Set or clear a breakpoint on the current line |
| **F8** | Start debugging / continue to the next breakpoint |
| F10 | Execute one instruction |
| Ctrl+O / Ctrl+S | Open / save |
| Ctrl+Space | Autocomplete |
| Ctrl+= / Ctrl+- | Zoom |

Program output and build errors show up in the console. Click an error to jump to its line. When your program reads from stdin, type in the line under the console output: Enter sends a line, Ctrl+D ends the input.

Your program runs as a Linux program, so it talks to the system through `svc #0`. See [examples/hello.s](examples/hello.s):

```asm
    mov     x0, #1          // stdout
    ldr     x1, =message    // text
    mov     x2, #length     // length
    mov     x8, #64         // write
    svc     #0
```

Building `foo.s` creates `foo.o` and the program `foo` next to it.

## Other Linux distributions

`run.sh` only knows how to install packages with `apt`. On other distributions, install these yourself:

- clang 18 or newer, CMake 3.24 or newer, Ninja, pkg-config, git
- GNU binutils for `aarch64-linux-gnu` (the assembler and linker)
- QEMU user mode (`qemu-aarch64`)
- development packages for GLFW 3, OpenGL, FreeType and libvterm

Then build and run by hand:

```bash
cmake --preset release
cmake --build --preset release
./build/arm_ide examples/hello.s
```
