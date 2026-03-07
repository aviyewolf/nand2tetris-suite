# Nand2Tetris Suite — User Manual

Welcome! This manual will guide you through installing and using the Nand2Tetris Suite tools. It is written for students who have completed (or are working through) the Nand2Tetris course. No prior experience with command-line tools is assumed — every step is explained in detail.

---

## Table of Contents

1. [What Is This Suite?](#1-what-is-this-suite)
2. [Installation](#2-installation)
   - [Windows](#windows)
   - [macOS](#macos)
   - [Linux](#linux)
3. [Your First 5 Minutes](#3-your-first-5-minutes)
4. [Tool 1: HDL Simulator (`hdl_sim`)](#4-tool-1-hdl-simulator-hdl_sim)
5. [Tool 2: CPU Simulator (`cpu_sim`)](#5-tool-2-cpu-simulator-cpu_sim)
6. [Tool 3: VM Emulator (`vm_emu`)](#6-tool-3-vm-emulator-vm_emu)
7. [Tool 4: Jack Debugger (`jack_debug`)](#7-tool-4-jack-debugger-jack_debug)
8. [Web Interface](#8-web-interface)
9. [Common Questions & Troubleshooting](#9-common-questions--troubleshooting)
10. [Glossary](#10-glossary)

---

## 1. What Is This Suite?

When you work through the Nand2Tetris course, you build several tools:

- An **assembler** that turns `.asm` files into `.hack` machine code
- A **VM translator** that turns `.vm` files into assembly
- A **Jack compiler** that turns `.jack` programs into VM code
- **HDL chip designs** (`.hdl` files) that define digital logic

This suite lets you **test and debug the output** of those tools. Think of it as a set of "virtual machines" that can run each file format and show you exactly what happens.

| Tool | What It Runs | When To Use It |
|------|-------------|----------------|
| `hdl_sim` | `.hdl` chip designs | Testing chips from Projects 01-03 |
| `cpu_sim` | `.hack` machine code | Testing assembler output (Projects 04, 06) |
| `vm_emu` | `.vm` VM programs | Testing VM translator or compiler output (Projects 07-08, 10-11) |
| `jack_debug` | `.vm` + `.smap` or `.jack` files | Debugging Jack programs at source level (Projects 09-11) |

---

## 2. Installation

### What You Need

Before you start, make sure you have these programs installed on your computer:

- **A C++ compiler** — this is a program that turns source code into programs your computer can run
- **CMake** — a tool that manages the build process
- **Git** — for downloading the source code

Don't worry if you have never used these before — follow the steps below for your operating system.

---

### Windows

**Step 1: Install the tools**

The easiest way is to install [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/) (free). During installation, check the box **"Desktop development with C++"**. This gives you a C++ compiler and CMake.

Alternatively, if you prefer a lighter setup:
1. Install [Git for Windows](https://git-scm.com/download/win)
2. Install [CMake](https://cmake.org/download/) — download the Windows installer, and **check "Add CMake to the system PATH"**
3. Install [MinGW-w64](https://www.mingw-w64.org/) or use WSL (Windows Subsystem for Linux)

**Step 2: Download the suite**

Open a terminal (Command Prompt, PowerShell, or Git Bash) and type:

```
git clone <repository-url> nand2tetris-suite
cd nand2tetris-suite
```

**Step 3: Build**

```
mkdir build
cd build
cmake ..
cmake --build .
```

If everything worked, you should see no errors. The programs are now in the `build/bin/` folder.

**Step 4: Verify**

```
bin\cpu_sim --help
```

You should see usage instructions printed to the screen. If you do — you are ready to go!

---

### macOS

**Step 1: Install the tools**

Open the **Terminal** app (you can find it in Applications > Utilities, or search for it with Spotlight).

Install the Xcode command-line tools (this gives you a C++ compiler):

```
xcode-select --install
```

Install CMake using [Homebrew](https://brew.sh/). If you don't have Homebrew, install it first:

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Then install CMake:

```
brew install cmake
```

**Step 2: Download the suite**

```
git clone <repository-url> nand2tetris-suite
cd nand2tetris-suite
```

**Step 3: Build**

```
mkdir build && cd build
cmake ..
cmake --build .
```

**Step 4: Verify**

```
./bin/cpu_sim --help
```

---

### Linux

**Step 1: Install the tools**

On Ubuntu/Debian:

```
sudo apt update
sudo apt install build-essential cmake git
```

On Fedora:

```
sudo dnf install gcc-c++ cmake git
```

**Step 2: Download and build**

```
git clone <repository-url> nand2tetris-suite
cd nand2tetris-suite
mkdir build && cd build
cmake ..
cmake --build .
```

**Step 3: Verify**

```
./bin/cpu_sim --help
```

---

## 3. Your First 5 Minutes

Let's do a quick test to make sure everything works. We will run a tiny VM program that adds two numbers.

**Open a terminal** and navigate to the suite folder:

```
cd nand2tetris-suite
```

**Run a simple program:**

```
./build/bin/vm_emu --run examples/vm/SimpleAdd.vm -n 100
```

You should see output like this:

```
[HALTED]
  3: add  [<global>]

Stack (SP=257, 1 entries):
  [256] 15 (0x000f)  <-- top

  Instructions executed: 3
  Push count:            2
  Pop count:             0
  Arithmetic count:      1
  Call count:            0
  Return count:          0
```

This tells you:
- The program **halted** normally (no errors)
- It pushed 7 and 8 onto the stack, then added them
- The result **15** is on top of the stack
- It executed **3** VM commands total

Congratulations — the suite is working!

---

## 4. Tool 1: HDL Simulator (`hdl_sim`)

### What it does

The HDL Simulator tests your chip designs from Projects 01-03. It loads an `.hdl` file (your chip), runs a `.tst` test script against it, and tells you if your chip produces the correct outputs.

### Batch mode — run a test script

This is the simplest way to use it. Just point it at a test file:

```
./build/bin/hdl_sim --test path/to/And.tst
```

To also check against an expected output file:

```
./build/bin/hdl_sim --test path/to/And.tst --compare path/to/And.cmp
```

**What you see:**

If the test passes:

```
| a | b |out|
| 0 | 0 | 0 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |
PASS (4 evaluations, 4 output rows)
```

If the test fails, you will see `FAIL` along with an error message explaining which output was wrong.

### Interactive mode — explore a chip by hand

Sometimes you want to poke at a chip directly — set inputs, evaluate, and read outputs. Interactive mode lets you do exactly that:

```
./build/bin/hdl_sim path/to/And.hdl
```

You will see a `>` prompt. Here is an example session:

```
HDL Simulator — And.hdl
Type 'help' for commands.

> set a 1
a = 1
> set b 1
b = 1
> eval
Evaluated.
> get out
out = 1
> set b 0
b = 0
> eval
Evaluated.
> get out
out = 0
> quit
```

### Interactive commands reference

| Command | What it does |
|---------|-------------|
| `set <pin> <value>` | Set an input pin to a value (0 or 1 for single-bit, or a number for buses) |
| `eval` | Evaluate the chip — calculates all outputs from the current inputs |
| `tick` | Clock rising edge (for sequential chips like Register, RAM) |
| `tock` | Clock falling edge (for sequential chips) |
| `get <pin>` | Read the value of an output pin |
| `pins` | List all pins and their current values |
| `reset` | Reset the chip (all pins back to 0) |
| `help` | Show available commands |
| `quit` or `q` | Exit |

### Tips for students

- **Sequential chips** (Register, RAM, PC) need `tick` then `tock` to complete one clock cycle. This is how data gets latched into the register.
- **Combinational chips** (And, Or, Mux, ALU, etc.) only need `eval` — they have no memory.
- If you are not sure whether your chip is sequential, try `tick` — if the chip does not support it, you will get an error message.

---

## 5. Tool 2: CPU Simulator (`cpu_sim`)

### What it does

The CPU Simulator runs `.hack` machine code files — the output of your assembler from Project 06. It simulates the entire Hack CPU: registers A and D, the program counter (PC), and 32K of RAM.

### Batch mode — run a program

```
./build/bin/cpu_sim --run path/to/Add.hack -n 1000
```

The `-n 1000` means "run for at most 1000 instructions, then stop." This prevents infinite loops from running forever. If you leave it out, the program runs until the PC goes past the end of ROM.

**What you see:**

```
[HALTED] PC past end of ROM
  A  = 0 (0x0000)
  D  = 5 (0x0005)
  PC = 6

  Instructions executed: 6
  A-instructions:        3
  C-instructions:        3
  Jumps taken:           0
  Memory reads:          0
  Memory writes:         1
```

This tells you the final state of the CPU after the program finished.

### Interactive mode — step through code

This is where the real debugging happens. Launch it with just the file name:

```
./build/bin/cpu_sim path/to/Add.hack
```

Here is an example debugging session:

```
Hack CPU Simulator — Add.hack
ROM: 6 instructions loaded
Type 'help' for commands.

> dasm 0 6
      0: @2          <-- PC
      1: D=A
      2: @3
      3: D=D+A
      4: @0
      5: M=D

> step
PC=1  D=A

> step
PC=2  D=D+A

> regs
  A  = 2 (0x0002)
  D  = 2 (0x0002)
  PC = 2

> run
[HALTED] PC past end of ROM
  A  = 0 (0x0000)
  D  = 5 (0x0005)
  PC = 6

> ram 0 1
  RAM[0] = 5 (0x0005)

> quit
```

### Interactive commands reference

| Command | What it does |
|---------|-------------|
| `step [N]` or `s [N]` | Execute the next N instructions (default: 1) |
| `run` or `r` | Run until the program halts or hits a breakpoint |
| `run N` | Run for at most N instructions |
| `regs` | Show the A, D, and PC registers |
| `ram <addr> [count]` | Show values stored in RAM |
| `rom <addr> [count]` | Show ROM contents with disassembly |
| `break <addr>` or `b <addr>` | Set a breakpoint at a ROM address |
| `clear <addr>` | Remove a breakpoint |
| `breaks` | List all breakpoints |
| `dasm [addr] [count]` | Disassemble instructions (default: 10 from PC) |
| `stats` | Show execution statistics |
| `reset` | Reset the CPU (clear registers and RAM, restart at PC=0) |
| `help` | Show available commands |
| `quit` or `q` | Exit |

### Tips for students

- **Use `dasm` first** to see what your program looks like in human-readable form. If it looks wrong, the bug is in your assembler.
- **Set breakpoints** with `break` to pause execution at interesting points, then inspect registers with `regs` and memory with `ram`.
- **The `-n` flag** is your safety net against infinite loops. Start with a small number like 1000, then increase it.
- **RAM[0]** is the stack pointer (SP). **RAM[1]** is LCL, **RAM[2]** is ARG, **RAM[3]** is THIS, **RAM[4]** is THAT. These are important when debugging VM translator output.

---

## 6. Tool 3: VM Emulator (`vm_emu`)

### What it does

The VM Emulator runs `.vm` files — the stack-based virtual machine code used in Projects 07-08. It can also run directories containing multiple `.vm` files (as in Project 08, where a program consists of several files).

### Batch mode — run a program

**Single file:**

```
./build/bin/vm_emu --run examples/vm/SimpleAdd.vm -n 100
```

**Directory with multiple `.vm` files:**

```
./build/bin/vm_emu --run path/to/FibonacciElement/ -n 10000
```

(Note the trailing `/` to indicate it is a directory.)

### Interactive mode — debug VM code

```
./build/bin/vm_emu examples/vm/StackTest.vm
```

Example debugging session:

```
VM Emulator — examples/vm/StackTest.vm
12 commands loaded
Type 'help' for commands.

> step
  1: push constant 17  [<global>]

> step
  2: push constant 17  [<global>]

> step
  3: eq  [<global>]

> stack
Stack (SP=257, 1 entries):
  [256] -1 (0xffff)  <-- top

> step 3
  6: push constant 16  [<global>]

> run
[HALTED]
  12: gt  [<global>]

> stack
Stack (SP=260, 4 entries):
  [256] -1 (0xffff)
  [257] 0 (0x0000)
  [258] 0 (0x0000)
  [259] -1 (0xffff)  <-- top

> quit
```

**Reading the stack output:**

- A value of `-1` (which is `0xFFFF`) means **true** in the Hack/VM world
- A value of `0` means **false**
- So the stack shows: `true, false, false, true` — which matches: 17==17 is true, 17==16 is false, 3>4 is false, 4>3 is true

### Interactive commands reference

| Command | What it does |
|---------|-------------|
| `step [N]` or `s [N]` | Step N VM commands (default: 1) |
| `over` or `n` | Step over a function call (execute the whole function, then stop) |
| `out` | Step out — run until the current function returns |
| `run` or `r` | Run until halt or breakpoint |
| `run N` | Run for N instructions |
| `stack` | Show the stack contents |
| `seg <name> [index]` | Show a memory segment: `local`, `argument`, `this`, `that`, `temp`, `static`, `pointer` |
| `ram <addr> [count]` | Show raw RAM values |
| `cmd [index]` | Show the current VM command, or a specific one by index |
| `calls` | Show the function call stack |
| `break <index>` or `b <index>` | Set a breakpoint at a VM command index |
| `fbreak <function>` | Set a breakpoint at a function entry (e.g., `fbreak Main.main`) |
| `clear <index>` | Remove a breakpoint |
| `breaks` | List all breakpoints |
| `stats` | Show execution statistics |
| `reset` | Reset the VM |
| `help` | Show available commands |
| `quit` or `q` | Exit |

### Tips for students

- **Use `stack` often.** The stack is the heart of the VM — if something looks wrong, check the stack first.
- **Use `seg`** to look at local variables and arguments. For example, `seg local` shows all local variables, `seg argument 0` shows the first argument.
- **Use `calls`** when your program has function calls. It shows you which function you are in and how you got there.
- **Use `over`** to skip over a function call you are not interested in. This is much faster than stepping through every instruction inside the function.
- **Use `fbreak`** to pause at a specific function. For example, `fbreak Math.multiply` pauses whenever that function is called.

---

## 7. Tool 4: Jack Debugger (`jack_debug`)

### What it does

The Jack Debugger is the most advanced tool in the suite. It lets you debug Jack programs at the **source code level** — seeing Jack file names, line numbers, and variable names — even though the program is actually running as VM code underneath.

There are two ways to provide source information:

**Option A: Using .jack source files (recommended)**
- A `.vm` file — the compiled VM code
- One or more `.jack` files — the original Jack source files

The debugger automatically generates a source map from your `.jack` files. This is the easiest approach — no extra tools needed.

**Option B: Using a .smap file**
- A `.vm` file — the compiled VM code
- A `.smap` file — a "source map" that maps VM commands back to Jack source lines

### Batch mode

```
./build/bin/jack_debug --run Program.vm Main.jack Point.jack -n 10000
./build/bin/jack_debug --run Program.vm Program.smap -n 10000
```

### Interactive mode

```
./build/bin/jack_debug Program.vm Main.jack Point.jack
./build/bin/jack_debug Program.vm Program.smap
```

Example session (using `.jack` files):

```
$ jack_debug Program.vm Main.jack Point.jack

Jack Debugger — Program.vm
Source: 2 .jack file(s) (auto source map)
Type 'help' for commands.

> step
  Main:3 in Main.main

> vars
  local int x = 0 (0x0000)
  local int y = 0 (0x0000)
  arg int n = 5 (0x0005)

> var n
  arg int n = 5 (0x0005)

> over
  Main:4 in Main.main

> calls
  0: Main.main (Main:4)

> break Main 10
Breakpoint set at Main:10

> run
[PAUSED] breakpoint
  Main:10 in Main.main

> vars
  local int x = 42 (0x002a)
  local int y = 7 (0x0007)
  arg int n = 5 (0x0005)

> quit
```

The same workflow works with `.smap` files:

```
$ jack_debug Program.vm Program.smap
```

### Interactive commands reference

| Command | What it does |
|---------|-------------|
| `step` or `s` | Step to the next Jack source line |
| `over` or `n` | Step over — execute a method call without stepping into it |
| `out` | Step out — run until the current method returns |
| `run` or `r` | Run until halt or breakpoint |
| `run N` | Run for N VM instructions |
| `where` | Show current Jack source location (file and line number) |
| `vars` | Show all variables currently in scope |
| `var <name>` | Inspect a specific variable by name |
| `eval <expr>` | Evaluate a simple expression |
| `inspect <addr> <class>` | Inspect an object on the heap |
| `array <addr> <length>` | Inspect an array on the heap |
| `this` | Inspect the current object (`this`) |
| `calls` | Show the Jack-level call stack |
| `break <file> <line>` or `b <file> <line>` | Set a breakpoint at a Jack source line |
| `clear <file> <line>` | Remove a breakpoint |
| `breaks` | List all breakpoints |
| `stats` | Show profiling statistics (per-function call and instruction counts) |
| `reset` | Reset the debugger |
| `help` | Show available commands |
| `quit` or `q` | Exit |

### How auto source maps work

When you pass `.jack` files instead of a `.smap` file, the debugger:

1. **Parses** each `.jack` file for declarations — class names, field/static variables, subroutine signatures, parameter lists, and local variables
2. **Scans** subroutine bodies for statement-starting keywords (`let`, `do`, `if`, `while`, `return`) and records their line numbers
3. **Correlates** these declarations with the compiled VM code by matching function names (e.g., `Main.main` appears in both the Jack source and the VM `function` command)
4. **Maps** VM command groups to Jack source lines using statement-boundary heuristics

This gives you variable names, types, and indices, source-line stepping, and class layouts for object inspection — all without modifying your compiler.

**Limitations:** The auto-generated source map uses heuristics to match VM commands to Jack source lines. In some cases (complex expressions, nested control flow), a VM command may map to the wrong Jack line. If you need exact line mappings, use a `.smap` file generated by a compiler that supports them.

### Tips for students

- **Start with `.jack` files** — just pass your Jack source files alongside the `.vm` file and you get source-level debugging immediately. No need to generate `.smap` files.
- **Use `where`** if you lose track of where you are in the code.
- **Use `vars`** to see all variables at once. This is the fastest way to check if your program is computing the right values.
- **Use `this`** when debugging methods — it shows you all the fields of the current object.
- **Use `inspect`** to look at objects on the heap. You need the memory address (you can get it from a variable's raw value) and the class name.
- **Use `stats`** to find performance bottlenecks — it shows how many times each function was called and how many instructions it used.

---

## 8. Web Interface

The suite includes a browser-based interface that lets you use all four tools without installing anything. It runs entirely in your browser using WebAssembly — the same C++ simulation engines compiled to run on the web.

### Getting started

Open the web interface in your browser (see the project repository for the hosted URL, or build it locally — see [Building the web interface](#building-the-web-interface) below). You will see four tabs along the top:

| Tab | Equivalent CLI tool | What it does |
|-----|---------------------|-------------|
| **HDL** | `hdl_sim` | Test chip designs |
| **CPU** | `cpu_sim` | Run and debug machine code |
| **VM** | `vm_emu` | Run and debug VM programs |
| **Jack** | `jack_debug` | Source-level Jack debugging |

Each tab has a built-in **code editor** on the left where you can paste code or upload files, and a **results panel** on the right showing simulation output.

### HDL tab

Use this tab to test your HDL chip designs from Projects 01-03.

**Loading files:**
- Paste your `.hdl` chip definition into the **HDL** editor panel
- Paste the `.tst` test script into the **Test Script** editor panel
- Optionally paste the `.cmp` comparison file into the **Compare** editor panel
- Or click the upload button in any editor panel to load files from disk

**Running tests:**
- Click **Run** to execute the entire test script at once
- Click **Step** to execute one test iteration at a time (useful for debugging sequential chips)
- The **output table** appears below, showing the test results in the same format as the CLI
- If a `.cmp` file is loaded, mismatches are reported as errors

**Status bar** shows the number of evaluations and output rows.

### CPU tab

Use this tab to run `.hack` machine code from Project 06.

**Loading code:**
- Paste your `.hack` binary (one instruction per line, 16 bits each) into the editor
- Or upload a `.hack` file

**Running and debugging:**
- **Step** — execute one instruction at a time
- **Run** — run continuously (executes in batches of 50,000 instructions for a responsive UI)
- **Pause** — stop continuous execution
- **Reset** — restart from the beginning

**Panels (right side):**
- **Disassembly** — shows your program in human-readable assembly, with the current PC highlighted
- **Registers** — shows the current values of A, D, and PC
- **RAM viewer** — browse memory contents; type an address in the input box and press Enter to jump to that location
- **Screen** — a 512x256 pixel display showing the Hack screen memory (RAM 16384-24575), updated in real time
- **Statistics** — instruction counts, memory operations, and jump counts

**Keyboard input:**
Click on the CPU tab area to give it focus, then type on your keyboard. Key presses are mapped to the Hack keyboard register (RAM 24576) so interactive programs (like the Hack OS) can read input.

### VM tab

Use this tab to run `.vm` programs from Projects 07-08.

**Loading code:**
- Paste your `.vm` code into the editor, or upload a `.vm` file

**Running and debugging:**
- **Step** — execute one VM command
- **Step Over** — execute a function call without stepping into it
- **Step Out** — run until the current function returns
- **Run** — run continuously
- **Pause** / **Reset** — stop or restart

**Panels (right side):**
- **Current state** — shows the program counter, current VM command, and which function you are in
- **Stack** — a live visualization of all values on the stack (from address 256 onward), with the top of stack clearly marked
- **Memory segments** — shows the base addresses of SP, LCL, ARG, THIS, and THAT
- **Call stack** — shows the chain of function calls with source locations
- **Screen** — the Hack screen display, updated in real time
- **Keyboard** — interactive keyboard input support

### Jack tab

Use this tab for source-level debugging of Jack programs (Projects 09-11).

**Loading files:**
- Paste the compiled `.vm` code into the **VM Code** editor
- Then either:
  - Paste the `.smap` source map into the **Source Map** editor, or
  - Upload `.jack` source files using the file picker below the Source Map editor (auto source map mode)
- When `.jack` files are loaded, the debugger automatically generates a source map from them

**Running and debugging:**
- **Step** — step to the next Jack source line
- **Step Over** — execute a method call without stepping into it
- **Step Out** — run until the current method returns
- **Run** / **Pause** / **Reset**

**Panels (right side):**
- **Source location** — shows the current Jack file name, line number, and function
- **Variables** — all in-scope variables with their types and values (local, argument, field)
- **Call stack** — the Jack-level call stack with source file locations

### Tips for the web interface

- **No installation required** — everything runs in your browser via WebAssembly
- **Drag and drop** — you can upload files by clicking the upload button in any editor panel
- **The editors support multiple files** — for the HDL tab, you need both the HDL and test script; for Jack, you need VM code plus either a source map or `.jack` source files
- **Jack auto source maps** — in the Jack tab, you can upload `.jack` source files instead of a `.smap` file. The debugger will automatically generate a source map, giving you variable names, types, and source-line stepping without extra tooling
- **Screen updates** — the CPU and VM tabs render the Hack screen in real time (refreshed every 100ms during continuous execution)
- **Keyboard focus** — click on the tab area before typing to ensure keyboard input reaches the simulator
- **Performance** — continuous execution runs in batches to keep the UI responsive; the browser version is slightly slower than the CLI tools but fast enough for all course projects

### Building the web interface

If you want to build and host the web interface locally:

**Prerequisites:**
- [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) — compiles the C++ engines to WebAssembly
- [Node.js](https://nodejs.org/) (version 18 or higher) — for the React frontend

**Build steps:**

```bash
# 1. Build the WebAssembly module
mkdir build-wasm && cd build-wasm
emcmake cmake .. -DBUILD_WEB=ON -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)

# 2. Copy the WASM output to the frontend
cp bin/n2t.js bin/n2t.wasm ../web/frontend/public/

# 3. Build and run the frontend
cd ../web/frontend
npm install
npm run dev
```

The development server will start (typically at `http://localhost:5173`). Open it in your browser.

---

## 9. Common Questions & Troubleshooting

### "command not found" when I try to run a tool

Make sure you are running the tool from the correct location. The tools are inside the `build/bin/` folder. You need to include the path:

```
./build/bin/cpu_sim --help
```

Or on Windows:

```
build\bin\cpu_sim --help
```

If you get "permission denied" on Linux/macOS, make the file executable:

```
chmod +x build/bin/cpu_sim
```

### "Cannot open file" error

Double-check the file path. A few things to try:

- Use the full path: `./build/bin/cpu_sim --run /home/me/projects/Add.hack`
- Make sure the file extension is correct (`.hack`, not `.asm`; `.vm`, not `.jack`)
- Check for typos in the file name

### "cmake: command not found"

CMake is not installed or not in your system PATH. See the [Installation](#2-installation) section for your operating system.

### The build fails with compiler errors

Make sure your C++ compiler supports C++17. Check your version:

```
g++ --version    # Should be 7 or higher
clang++ --version  # Should be 5 or higher
```

If your compiler is too old, update it using your system's package manager.

### My program runs forever / seems stuck

Your program might have an infinite loop. Use the `-n` flag to limit execution:

```
./build/bin/cpu_sim --run Prog.hack -n 10000
```

In interactive mode, you can press **Ctrl+C** to interrupt, or use `run N` instead of `run` to limit how many instructions execute.

### The output looks wrong — how do I find the bug?

1. **Start with interactive mode** — step through the program one instruction at a time
2. **Set breakpoints** at interesting points in your code
3. **Inspect state** (registers, stack, memory) after each step
4. **Compare with what you expect** — if a register has the wrong value, look at the last few instructions to see where it went wrong

### What does `-1` mean on the stack?

In the Hack/VM world, `-1` (which is `0xFFFF` or all ones in binary) represents **true**. And `0` represents **false**. This is the convention used by the Nand2Tetris platform.

### What is a `.smap` file?

A source map file that connects VM commands back to the original Jack source code. It can be generated by compilers that support source-level debugging. If you don't have one, you can use `.jack` source files instead — the debugger will auto-generate a source map from them. You can also debug at the VM level using `vm_emu`.

### Can I use these tools with the official Nand2Tetris test files?

Yes. The `.tst` test scripts and `.cmp` comparison files from the Nand2Tetris course should work directly with `hdl_sim --test`.

---

## 10. Glossary

These are terms you will encounter when using the tools:

| Term | Meaning |
|------|---------|
| **A register** | The address register in the Hack CPU. Holds a 16-bit value used as a data value or memory address. |
| **Breakpoint** | A marker you set on a specific instruction. When the program reaches that point, it pauses so you can inspect the state. |
| **D register** | The data register in the Hack CPU. Used for computation. |
| **Eval** | Evaluate a chip — calculate outputs from the current inputs. |
| **Halt** | When a program finishes normally (PC goes past the last instruction). |
| **HDL** | Hardware Description Language — the language used to describe chip designs. |
| **Hack** | The name of the computer platform built in the Nand2Tetris course. |
| **PC (Program Counter)** | Points to the next instruction to be executed. |
| **RAM** | Random Access Memory — the main memory of the computer (32,768 words). |
| **REPL** | Read-Eval-Print Loop — the interactive mode where you type commands and see results immediately. |
| **ROM** | Read-Only Memory — where the program instructions are stored. |
| **Segment** | A named region of VM memory: `local`, `argument`, `this`, `that`, `temp`, `static`, `pointer`, `constant`. |
| **Source map** | A file (`.smap`) that maps compiled VM commands back to the original Jack source lines. |
| **SP (Stack Pointer)** | RAM[0] — points to the next free slot on the stack. |
| **Stack** | The data structure used by the VM to store temporary values. Grows upward from RAM address 256. |
| **Step** | Execute exactly one instruction, then pause. |
| **Step over** | Execute a function call completely without going inside it. |
| **Step out** | Run until the current function finishes and returns to its caller. |
| **Tick/Tock** | The two phases of a clock cycle for sequential chips. `tick` = rising edge, `tock` = falling edge. |
| **VM** | Virtual Machine — the stack-based intermediate language between Jack and assembly. |
| **Word** | A 16-bit value — the basic unit of data in the Hack computer. |
