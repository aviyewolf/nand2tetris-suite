# CPU Simulator

Simulates the Hack CPU executing machine code (.hack files).

## Purpose

When students write an assembler (Project 6), they produce `.hack` files containing 16-bit binary instructions. This simulator executes those instructions and allows students to:

1. **Verify correctness** - Does their assembler produce working code?
2. **Debug assembly programs** - Step through execution, inspect registers
3. **Understand the hardware** - See how the CPU actually works

## Architecture

### Hack CPU Overview

The Hack CPU has a Harvard architecture with:
- **Instruction Memory (ROM)**: Read-only, holds the program
- **Data Memory (RAM)**: 32K words (addresses 0-32767)
- **Two registers**:
  - `A` register: 16-bit address/data register
  - `D` register: 16-bit data register
- **Program Counter (PC)**: Points to current instruction
- **ALU**: Performs arithmetic and logic operations

### Memory Map

```
0-15:     RAM (used by VM as stack pointer, local, argument, this, that)
16-255:   RAM (used by VM as static variables)
256-2047: RAM (stack space)
2048-16383: RAM (heap space for objects/arrays)
16384-24575: Screen memory (256x512 pixels, black & white)
24576:    Keyboard memory (currently pressed key)
```

### Instruction Format

The Hack CPU has two instruction types:

#### A-Instruction (Address)
```
Format: 0vvvvvvvvvvvvvvv
Effect: A = vvvvvvvvvvvvvvv (15-bit value)
```

Sets the A register to a value. Used for:
- Loading constants: `@42` → A = 42
- Setting addresses: `@100` → A = 100, then M can access RAM[100]
- Jump targets: `@LOOP` → A = address of LOOP label

#### C-Instruction (Compute)
```
Format: 111accccccdddjjj

Where:
  a     = ALU input selector (0 = use A register, 1 = use M = RAM[A])
  cccccc = ALU operation (what to compute)
  ddd   = destination (where to store result)
  jjj   = jump condition (when to jump to address in A)
```

Examples:
- `D=D+1` → Increment D register
- `M=D` → Store D into RAM[A]
- `D;JGT` → Jump to address in A if D > 0

## Files

### Header Files

- **`instruction.hpp`** - Instruction decoder
  - Parses 16-bit instruction words
  - Extracts fields (a, comp, dest, jump)
  - Provides human-readable disassembly

- **`cpu.hpp`** - CPU simulator
  - Executes instructions
  - Manages registers (A, D, PC)
  - Handles control flow (jumps)

- **`memory.hpp`** - Memory subsystem
  - RAM (read/write)
  - ROM (read-only, holds program)
  - Screen memory (with pixel visualization)
  - Keyboard input

- **`debugger.hpp`** - Debug interface
  - Breakpoints (address-based and conditional)
  - Step execution (single instruction, run until breakpoint)
  - State inspection (registers, memory, stack)
  - Execution history

### Implementation Files

- **`instruction.cpp`** - Instruction decoder implementation
- **`cpu.cpp`** - CPU simulation engine
- **`memory.cpp`** - Memory management
- **`debugger.cpp`** - Debugger features

## Usage Example

```cpp
#include "cpu.hpp"
#include "debugger.hpp"

// Load a program
CPU cpu;
cpu.load_program("MyProgram.hack");

// Create a debugger
CPUDebugger debugger(cpu);

// Set a breakpoint at instruction 10
debugger.add_breakpoint(10);

// Run until breakpoint
debugger.run();

// Inspect state
std::cout << "A = " << cpu.get_A() << std::endl;
std::cout << "D = " << cpu.get_D() << std::endl;
std::cout << "PC = " << cpu.get_PC() << std::endl;

// Step one instruction
debugger.step();

// View memory
std::cout << "RAM[256] = " << cpu.get_memory(256) << std::endl;
```

## Design Considerations

### Performance

The CPU simulator needs to be fast because:
- Programs may execute millions of instructions
- Students may run graphics programs that need real-time execution
- Debugger should feel responsive

**Optimization strategies**:
- Instruction decoding is cached (decode once, execute many times)
- Separate fast path (run) and slow path (step with debug checks)
- Memory access is direct (no indirection)

### Accuracy

The simulator must behave exactly like the real Hack CPU:
- Arithmetic is 16-bit two's complement
- No undefined behavior (all instruction patterns are valid)
- Memory access wraps around (address & 0x7FFF)
- Instruction execution is atomic

### Debugging Features

Help students find bugs in their assembly code:
- **Breakpoints**: Stop at specific instructions
- **Watchpoints**: Stop when memory changes
- **Trace**: Show execution history
- **Disassembly**: Reverse machine code to mnemonics
- **Memory view**: Inspect RAM contents
- **Screen view**: Visualize screen memory as pixels
