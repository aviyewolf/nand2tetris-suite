# VM Emulator

Simulates the stack-based virtual machine that executes .vm files.

## Purpose

The VM (Virtual Machine) is a key abstraction layer in nand2tetris:
- **Jack Compiler** (Project 11) produces `.vm` files
- **VM Translator** (Projects 7-8) translates `.vm` to `.asm`

This emulator lets students:
1. **Test their Jack Compiler** - Run .vm output directly without needing their VM translator
2. **Test their VM Translator** - Compare emulator behavior vs. their translator output
3. **Debug VM code** - Step through VM commands, visualize stack operations
4. **Understand the abstraction** - See how high-level code maps to stack operations

## VM Architecture

### Stack Machine Model

The VM is a stack-based architecture:
- **Stack**: Holds temporary values, function parameters, return values
- **Memory Segments**: Different areas of memory for different purposes
- **Function Call Stack**: Tracks function calls with saved state

### Memory Segments

The VM provides 8 virtual memory segments:

```
┌──────────────┬─────────────┬────────────────────────────────────┐
│ Segment      │ Symbol      │ Purpose                            │
├──────────────┼─────────────┼────────────────────────────────────┤
│ local        │ LCL         │ Function local variables           │
│ argument     │ ARG         │ Function arguments                 │
│ this         │ THIS        │ Current object fields (pointer)    │
│ that         │ THAT        │ Array elements (pointer)           │
│ constant     │ -           │ Read-only constants (0-32767)      │
│ static       │ -           │ Global variables (per .vm file)    │
│ temp         │ -           │ Temporary variables (temp 0-7)     │
│ pointer      │ -           │ Access THIS/THAT pointers directly │
└──────────────┴─────────────┴────────────────────────────────────┘
```

### VM Command Types

#### 1. Stack Operations
```
push segment index    // Push segment[index] onto stack
pop segment index     // Pop stack into segment[index]
```

Examples:
```
push constant 10      // Push 10 onto stack
push local 0          // Push local variable 0 onto stack
pop argument 2        // Pop stack into argument 2
```

#### 2. Arithmetic/Logical Operations
```
add    // Pop two values, push sum
sub    // Pop y, pop x, push x-y
neg    // Pop y, push -y
eq     // Pop two values, push -1 if equal, 0 if not
gt     // Pop y, pop x, push -1 if x>y, 0 otherwise
lt     // Pop y, pop x, push -1 if x<y, 0 otherwise
and    // Pop two values, push bitwise AND
or     // Pop two values, push bitwise OR
not    // Pop value, push bitwise NOT
```

#### 3. Program Flow
```
label LABEL_NAME      // Define a label
goto LABEL_NAME       // Unconditional jump
if-goto LABEL_NAME    // Pop stack, jump if value is -1 (true)
```

#### 4. Function Calls
```
function functionName nVars    // Define function with nVars local variables
call functionName nArgs        // Call function with nArgs arguments
return                         // Return from function
```

## Files

### Header Files

- **`vm_engine.hpp`** - Main VM execution engine
  - Executes VM commands
  - Manages stack and segments
  - Handles function calls

- **`vm_stack.hpp`** - Stack implementation
  - Push/pop operations
  - Stack overflow/underflow detection
  - Stack visualization support

- **`vm_memory.hpp`** - Memory segment management
  - Local, argument, this, that segments
  - Static, temp, pointer segments
  - Constant segment (special case)

- **`vm_parser.hpp`** - VM command parser
  - Parse .vm files
  - Extract command type and arguments
  - Error reporting with line numbers

- **`vm_debugger.hpp`** - VM debugger
  - Step through VM commands
  - Breakpoints on commands or functions
  - Inspect stack and segments
  - Call stack visualization

### Implementation Files

- **`vm_engine.cpp`** - VM execution implementation
- **`vm_stack.cpp`** - Stack operations
- **`vm_memory.cpp`** - Segment management
- **`vm_parser.cpp`** - VM file parsing
- **`vm_debugger.cpp`** - Debug features

## Usage Example

```cpp
#include "vm_engine.hpp"
#include "vm_debugger.hpp"

// Create VM engine
VMEngine vm;

// Load a program
vm.load_file("Main.vm");
vm.load_file("Math.vm");  // Can load multiple .vm files

// Create debugger
VMDebugger debugger(vm);

// Set breakpoint on a function
debugger.add_function_breakpoint("Main.main");

// Run until breakpoint
debugger.run();

// Inspect stack
auto stack_contents = debugger.get_stack();
for (size_t i = 0; i < stack_contents.size(); ++i) {
    std::cout << "Stack[" << i << "] = " << stack_contents[i] << std::endl;
}

// Step one VM command
debugger.step();

// View segments
std::cout << "local 0 = " << vm.get_segment(SegmentType::LOCAL, 0) << std::endl;
std::cout << "argument 1 = " << vm.get_segment(SegmentType::ARGUMENT, 1) << std::endl;
```

## Key Implementation Details

### Stack Pointer (SP)

The stack grows upward from address 256:
```
SP points to the next free location
Initial: SP = 256
After push: SP = 257
After another push: SP = 258
After pop: SP = 257
```

### Segment Mapping

How VM segments map to Hack memory:

```
local:    RAM[LCL + index]    // LCL points to base of local segment
argument: RAM[ARG + index]    // ARG points to base of argument segment
this:     RAM[THIS + index]   // THIS points to current object
that:     RAM[THAT + index]   // THAT points to array
static:   RAM[16 + index]     // Static variables start at address 16
temp:     RAM[5 + index]      // Temp 0-7 are at addresses 5-12
pointer:  THIS/THAT directly  // pointer 0 = THIS, pointer 1 = THAT
constant: index itself        // constant 42 is just the value 42
```

### Function Call Convention

When calling a function:
1. Push arguments onto stack
2. Push return address
3. Push caller's LCL, ARG, THIS, THAT
4. Set ARG to point to first argument
5. Set LCL to point to first local variable
6. Jump to function

When returning:
1. Pop return value to argument 0
2. Restore SP, THIS, THAT, ARG, LCL
3. Jump to return address

This is all handled automatically by the VM.

## Performance Considerations

The VM emulator must be **very fast** because:
- Jack programs execute thousands of VM commands per frame
- Graphics programs need real-time performance (60fps)
- Even simple programs can execute millions of commands

**Optimization strategies**:
1. **Command dispatch**: Use function pointers or jump table, not switch
2. **Inline hot paths**: Stack operations should be inline
3. **Lazy evaluation**: Only compute debug info when debugger is active
4. **Memory pooling**: Pre-allocate stack and segments
5. **Instruction caching**: Parse each command once

### Fast vs. Debug Mode

Two execution modes:
- **Fast mode**: Just execute, no debug overhead
- **Debug mode**: Check breakpoints, update visualizations, track history

The emulator should be 10-100x faster in fast mode.

## Debugging Features

### Stack Visualization

Show the stack graphically:
```
┌─────┐
│ 42  │ ← SP
├─────┤
│ 100 │
├─────┤
│ 7   │
├─────┤
│ ... │
```

### Segment Inspection

View all segments at once:
```
local:    [15, 3, 0, 0, 0]
argument: [42, 100]
this:     [Memory at 3000]
that:     [Memory at 4000]
static:   [1, 2, 0, 0]
temp:     [0, 0, 0, 0, 0, 0, 0, 0]
```

### Call Stack

Show function call hierarchy:
```
Main.main (line 5)
  → Math.multiply (line 15)
    → Math.abs (line 3)  ← current
```

### Execution Trace

Show recent VM commands:
```
push constant 10
push constant 20
call Math.multiply 2  ← current
```

## Testing Strategy

The VM emulator should be tested against:
1. **Known correct outputs**: Compare with reference implementation
2. **Edge cases**: Stack overflow, invalid memory access
3. **Performance benchmarks**: Must handle large programs efficiently
4. **All VM commands**: Every command should be tested
