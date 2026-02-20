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

### 1. CPU Simulator (`core/cpu/`)

Simulates the Hack CPU executing machine code.

**Input**: `.hack` files (binary machine code from student's assembler)

**Features**:
- Execute Hack machine instructions
- Step through code instruction-by-instruction
- Breakpoints on memory addresses or conditions
- Inspect registers (A, D, PC)
- View memory (RAM, ROM, Screen buffer, Keyboard)
- Disassemble machine code back to mnemonics
- Execution history and performance stats

**Use Case**: When a student's assembler produces .hack files, they can run them here to verify correctness and debug issues.

### 2. VM Emulator (`core/vm/`)

Simulates the stack-based virtual machine.

**Input**: `.vm` files (from student's Jack compiler OR VM code they wrote)

**Features**:
- Execute VM commands (push, pop, add, sub, eq, lt, gt, etc.)
- Step through VM commands one at a time
- Visualize stack operations (push/pop)
- Inspect memory segments (local, argument, this, that, temp, static, pointer, constant)
- Track function calls and returns
- View heap memory (objects and arrays)
- Call stack with function names

**Use Case**: Debug VM code output from Jack compiler, or test VM translator by running .vm files.

### 3. Jack Debugger (`core/jack/`)

Source-level debugger for Jack programs.

**Input**: `.jack` source files + `.vm` files + source map

**Features**:
- Step through Jack source code (step over/into/out)
- Set breakpoints in Jack source
- Inspect variables by their Jack names (not VM segments)
- View objects and arrays as Jack types
- See call stack with Jack function names
- Evaluate Jack expressions in debug console
- Performance profiling

**Critical**: Must be extremely fast since Jack programs (especially graphics/games) execute thousands of VM instructions per frame.

### 4. HDL Simulator (`core/hdl/`)

Simulates digital logic chips.

**Input**: `.hdl` files (student's chip designs) + `.tst` test scripts

**Features**:
- Parse and simulate HDL chip definitions
- Run test scripts and compare outputs
- Visualize chip internals (gates, pins, connections)
- Show signal propagation
- Generate timing diagrams
- Report mismatches with expected outputs

**Use Case**: Test student's chip implementations for projects 1-5.

## Technology Stack

### Core Engine (C++)

Performance-critical simulation engines written in C++17:
- **Fast execution**: Handle 6M+ VM instructions/second for graphics programs
- **Low latency**: Real-time debugging without noticeable lag
- **Memory efficient**: Simulate large programs without overhead

### CLI Tools

Command-line interfaces for each simulator:
- Batch testing and automation
- Scripting and CI/CD integration
- Headless execution for grading

### Web Interface (TypeScript + React)

Browser-based UI for interactive debugging:
- Code editor with syntax highlighting
- Visual debuggers with state inspection
- Real-time visualizations (stack, memory, CPU state)
- Cross-platform (works anywhere)

**Bridge**: C++ core compiled to WebAssembly for browser execution

## Project Structure

```
nand2tetris-suite/
├── README.md                   # This file
├── CMakeLists.txt             # Build configuration
├── .gitignore                 # Git ignore rules
│
├── core/                      # C++ simulation engines (performance-critical)
│   ├── common/               # Shared utilities and base classes
│   │   ├── types.hpp         # Common type definitions
│   │   ├── error.hpp         # Error handling
│   │   └── parser.hpp        # Base parser utilities
│   │
│   ├── cpu/                  # CPU Simulator
│   │   ├── cpu.hpp/.cpp      # Hack CPU implementation
│   │   ├── memory.hpp/.cpp   # RAM, ROM, Screen, Keyboard
│   │   ├── instruction.hpp   # Instruction decoder
│   │   └── debugger.hpp/.cpp # CPU debugger interface
│   │
│   ├── vm/                   # VM Emulator
│   │   ├── vm_engine.hpp/.cpp        # VM execution engine
│   │   ├── vm_stack.hpp/.cpp         # Stack machine
│   │   ├── vm_memory.hpp/.cpp        # Memory segments
│   │   ├── vm_parser.hpp/.cpp        # VM command parser
│   │   └── vm_debugger.hpp/.cpp      # VM debugger
│   │
│   ├── jack/                 # Jack Debugger (maps VM to Jack source)
│   │   ├── source_map.hpp/.cpp       # Jack ↔ VM mapping
│   │   ├── jack_debugger.hpp/.cpp    # Source-level debugger
│   │   └── object_inspector.hpp/.cpp # Object/array visualization
│   │
│   ├── hdl/                  # HDL Simulator
│   │   ├── chip.hpp/.cpp             # Chip representation
│   │   ├── hdl_parser.hpp/.cpp       # HDL file parser
│   │   ├── simulator.hpp/.cpp        # Chip simulation engine
│   │   └── test_runner.hpp/.cpp      # Test script executor
│   │
│   └── api/                  # C API for language bindings
│       ├── cpu_api.h/.cpp
│       ├── vm_api.h/.cpp
│       ├── jack_api.h/.cpp
│       └── hdl_api.h/.cpp
│
├── cli/                       # Command-line tools
│   ├── cpu_sim.cpp           # CPU simulator CLI
│   ├── vm_emu.cpp            # VM emulator CLI
│   ├── jack_debug.cpp        # Jack debugger CLI
│   └── hdl_sim.cpp           # HDL simulator CLI
│
├── web/                       # Web interface
│   ├── frontend/             # React/TypeScript UI
│   │   ├── src/
│   │   │   ├── components/   # Reusable UI components
│   │   │   ├── editors/      # Code editors
│   │   │   ├── debuggers/    # Debugger UIs
│   │   │   └── visualizers/  # Visualization components
│   │   └── package.json
│   │
│   └── wasm/                 # WebAssembly bindings
│       └── bindings.cpp      # C++ → WASM bridge
│
├── tests/                     # Test suite
│   ├── unit/                 # Unit tests (Google Test)
│   └── integration/          # Integration tests
│
├── examples/                  # Example programs for testing
│   ├── asm/                  # Assembly examples
│   ├── vm/                   # VM code examples
│   └── jack/                 # Jack program examples
│
└── docs/                      # Documentation
    ├── ARCHITECTURE.md       # Detailed architecture
    ├── API.md               # API documentation
    └── USER_GUIDE.md        # User guide
```

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+
- Git

### Build Instructions

```bash
# Clone and navigate to project
cd nand2tetris-suite

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run tests
ctest
```

## Development Phases

### Phase 1: Foundation (Current)
- [x] Project structure
- [x] Build system setup
- [ ] Common utilities
- [ ] Basic parsers

### Phase 2: CPU Simulator
- [ ] Instruction decoder
- [ ] CPU execution engine
- [ ] Memory subsystem
- [ ] Basic debugger
- [ ] CLI tool

### Phase 3: VM Emulator
- [ ] VM command parser
- [ ] Stack machine
- [ ] Memory segments
- [ ] VM debugger
- [ ] CLI tool

### Phase 4: Jack Debugger
- [ ] Source map parser
- [ ] Source-level debugging
- [ ] Variable inspector
- [ ] CLI tool

### Phase 5: HDL Simulator
- [ ] HDL parser
- [ ] Chip simulator
- [ ] Test runner
- [ ] CLI tool

### Phase 6: Web Interface
- [ ] WebAssembly compilation
- [ ] Code editors
- [ ] Debugger UI
- [ ] Visualizations

### Phase 7: Polish
- [ ] Performance optimization
- [ ] Documentation
- [ ] Example programs
- [ ] User guides

## Design Principles

1. **Understandability**: Code should be clear and self-documenting
   - Meaningful variable and function names
   - Comprehensive comments explaining WHY, not just WHAT
   - Clear separation of concerns

2. **Performance**: Especially for VM/Jack execution
   - C++ for hot paths
   - Efficient data structures
   - Separate fast (run) vs. debug (step) modes

3. **Modularity**: Each component is independent
   - Clear interfaces between layers
   - Can use CPU simulator without VM emulator
   - Can use VM emulator without Jack debugger

4. **Testability**: Everything is unit tested
   - Comprehensive test coverage
   - Integration tests for full stack
   - Example programs for validation

## Contributing

This is an educational project. Code should be:
- Well-documented with clear explanations
- Easy to understand for students learning systems programming
- Performance-conscious where needed
- Following C++ best practices

## License

[To be determined]

## Resources

- [Nand2Tetris Course](https://www.nand2tetris.org/)
- [Nand2Tetris Textbook](https://www.nand2tetris.org/book)
- [Course Projects](https://www.nand2tetris.org/course)
