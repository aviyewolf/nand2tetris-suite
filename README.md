# Nand2Tetris Educational Suite

A comprehensive simulation, testing, and debugging platform for students who have completed the [Nand2Tetris](https://www.nand2tetris.org/) course.

## Purpose

This suite helps students understand and debug their implementations by providing:
- **Simulators** - Run the output of student-built tools (assemblers, VM translators, compilers)
- **Debuggers** - Step through execution, inspect state, find bugs
- **Visualizers** - See how their code executes at different abstraction levels

**Important**: This suite does NOT implement the course projects (assembler, VM translator, compiler). Instead, it simulates and debugs the OUTPUT that students' implementations produce.

## Architecture Overview

The nand2tetris course has students build a complete computer system in layers:

```
┌─────────────────────────────────────────────────────┐
│  Jack Programs (.jack files)                        │
│  High-level OOP language                            │
└─────────────────────────────────────────────────────┘
                    ↓ [Student's Jack Compiler]
┌─────────────────────────────────────────────────────┐
│  VM Code (.vm files)                                │
│  Stack-based virtual machine commands               │
└─────────────────────────────────────────────────────┘
                    ↓ [Student's VM Translator]
┌─────────────────────────────────────────────────────┐
│  Hack Assembly (.asm files)                         │
│  Symbolic assembly language                         │
└─────────────────────────────────────────────────────┘
                    ↓ [Student's Assembler]
┌─────────────────────────────────────────────────────┐
│  Machine Code (.hack files)                         │
│  16-bit binary instructions                         │
└─────────────────────────────────────────────────────┘
                    ↓ [Hardware Platform]
┌─────────────────────────────────────────────────────┐
│  Hack Computer (built from .hdl chip designs)       │
│  CPU + Memory + I/O                                 │
└─────────────────────────────────────────────────────┘
```

### What This Suite Provides

1. **HDL Simulator** - Simulates chip designs (.hdl → working chips)
2. **CPU Simulator** - Executes machine code (.hack → running programs)
3. **VM Emulator** - Runs VM code (.vm → stack machine execution)
4. **Jack Debugger** - Source-level debugging of Jack programs

## Components

### 1. HDL Simulator (`core/hdl/`)

Simulates digital logic chips from projects 01-03.

**Input**: `.hdl` files (student's chip designs) + `.tst` test scripts

**Features**:
- Parse and simulate HDL chip definitions
- All combinational built-in chips (Nand, And, Or, Xor, Mux, DMux, 16-bit variants, multi-way, arithmetic, ALU)
- All sequential built-in chips (DFF, Bit, Register, RAM8/64/512/4K/16K, PC)
- Clock-phase simulation (tick/tock) for sequential logic
- Run .tst test scripts with comparison output
- Support for user-defined composite chips with topological evaluation order
- Bus subscripts and true/false constants

**Use Case**: Test student's chip implementations for projects 01-03.

### 2. CPU Simulator (`core/cpu/`)

Simulates the Hack CPU executing machine code.

**Input**: `.hack` files (binary machine code from student's assembler)

**Features**:
- Execute Hack machine instructions (A-instructions, C-instructions)
- Step through code instruction-by-instruction
- Breakpoints on memory addresses or conditions
- Inspect registers (A, D, PC)
- View memory (RAM, ROM, Screen buffer, Keyboard)
- Disassemble machine code back to mnemonics
- Execution history and performance stats

**Use Case**: When a student's assembler produces .hack files, they can run them here to verify correctness and debug issues.

### 3. VM Emulator (`core/vm/`)

Simulates the stack-based virtual machine.

**Input**: `.vm` files (from student's Jack compiler OR VM code they wrote)

**Features**:
- Execute VM commands (push, pop, add, sub, eq, lt, gt, etc.)
- Step through VM commands one at a time
- Inspect memory segments (local, argument, this, that, temp, static, pointer, constant)
- Track function calls and returns with call stack
- Breakpoints at VM command indices
- Run-for-N-instructions mode
- Performance statistics

**Use Case**: Debug VM code output from Jack compiler, or test VM translator by running .vm files.

### 4. Jack Debugger (`core/jack/`)

Source-level debugger for Jack programs.

**Input**: `.vm` files + `.smap` source map files

**Features**:
- Step through Jack source code (step / step over / step out)
- Set breakpoints at Jack source lines (translated to VM breakpoints)
- Inspect local variables, arguments, fields, and statics by their Jack names
- View objects and arrays as Jack types via heap inspection
- Jack-level call stack with source file and line info
- Evaluate simple expressions (integer literals and variable names)
- Per-function instruction counting and profiling stats

**Use Case**: Debug Jack programs at the source level rather than raw VM commands.

## Project Structure

```
nand2tetris-suite/
├── CMakeLists.txt              # Root build configuration
├── README.md
│
├── core/                       # C++ simulation engines
│   ├── common/                 # Shared utilities
│   │   ├── types.hpp/.cpp      # Common type definitions (Word, Address, etc.)
│   │   └── error.hpp           # Error hierarchy (ParseError, RuntimeError, etc.)
│   │
│   ├── hdl/                    # HDL Simulator
│   │   ├── hdl_parser.hpp/.cpp     # HDL file parser (CHIP, IN, OUT, PARTS, BUILTIN, CLOCKED)
│   │   ├── hdl_chip.hpp/.cpp       # Chip runtime (pins, eval, tick/tock, wiring, topo sort)
│   │   ├── hdl_builtins.hpp/.cpp   # Built-in chips (gates, arithmetic, ALU, DFF, RAM, PC)
│   │   ├── hdl_engine.hpp/.cpp     # Top-level simulation orchestrator
│   │   └── tst_runner.hpp/.cpp     # .tst test script executor
│   │
│   ├── cpu/                    # CPU Simulator
│   │   ├── cpu.hpp/.cpp            # Hack CPU execution engine
│   │   ├── memory.hpp/.cpp         # RAM, ROM, Screen, Keyboard
│   │   └── instruction.hpp/.cpp    # Instruction decoder
│   │
│   ├── vm/                     # VM Emulator
│   │   ├── vm_engine.hpp/.cpp      # VM execution engine
│   │   ├── vm_memory.hpp/.cpp      # Memory segments
│   │   ├── vm_parser.hpp/.cpp      # VM command parser
│   │   └── vm_command.hpp/.cpp     # VM command types
│   │
│   ├── jack/                   # Jack Debugger
│   │   ├── source_map.hpp/.cpp         # .smap parser: Jack ↔ VM mapping + symbol tables
│   │   ├── jack_debugger.hpp/.cpp      # Source-level debugger (step/breakpoint/inspect)
│   │   └── object_inspector.hpp/.cpp   # Heap object and array visualization
│   │
│   └── api/                    # C API for language bindings (planned)
│
├── tests/                      # Test suite
│   ├── hdl_engine_test.cpp     # HDL simulator tests (280 tests)
│   ├── cpu_engine_test.cpp     # CPU simulator tests (74 tests)
│   ├── vm_engine_test.cpp      # VM emulator tests
│   └── jack_debugger_test.cpp  # Jack debugger tests (120 tests)
│
├── cli/                        # Command-line tools (planned)
├── web/                        # Web interface (planned)
│   ├── frontend/               # React/TypeScript UI
│   └── wasm/                   # WebAssembly bindings
├── examples/                   # Example programs (planned)
│   ├── asm/                    # Assembly examples
│   ├── vm/                     # VM code examples
│   └── jack/                   # Jack program examples
└── docs/                       # Documentation (planned)
```

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+
- Git

### Build Instructions

```bash
cd nand2tetris-suite
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Tests

```bash
# Run all tests
./build/bin/hdl_engine_test      # 280 tests - HDL simulator
./build/bin/cpu_engine_test      # 74 tests  - CPU simulator
./build/bin/vm_engine_test       # VM emulator tests
./build/bin/jack_debugger_test   # 120 tests - Jack debugger
```

## Development Status

### Core Engines - Complete

- [x] **Common utilities** - Types, error hierarchy
- [x] **HDL Simulator** - Parser, chip runtime, all combinational + sequential builtins, .tst runner
  - [x] Phase 1: Combinational chips (projects 01-02)
  - [x] Phase 2: Sequential chips (project 03) - DFF, Bit, Register, RAM, PC, tick/tock
- [x] **CPU Simulator** - Instruction decoder, memory subsystem, CPU execution engine
- [x] **VM Emulator** - VM command parser, memory segments, stack machine, function calls
- [x] **Jack Debugger** - Source map parser, source-level stepping, variable/object inspection, breakpoints

### Planned

- [ ] CLI tools (cpu_sim, vm_emu, jack_debug, hdl_sim)
- [ ] C API bindings for language interop
- [ ] WebAssembly compilation
- [ ] Web frontend (React/TypeScript)
- [ ] Example programs
- [ ] Documentation

## Design Principles

1. **Understandability**: Code should be clear and self-documenting
2. **Performance**: C++ for hot paths, efficient data structures
3. **Modularity**: Each component is independent with clear interfaces
4. **Testability**: Comprehensive test coverage for all engines

## Resources

- [Nand2Tetris Course](https://www.nand2tetris.org/)
- [Nand2Tetris Textbook](https://www.nand2tetris.org/book)
- [Course Projects](https://www.nand2tetris.org/course)
