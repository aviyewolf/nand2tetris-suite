// ==============================================================================
// VM Command Representation
// ==============================================================================
// This file defines how we represent VM commands internally.
// Each command from a .vm file becomes a VMCommand struct that the
// execution engine can process efficiently.
// ==============================================================================

#ifndef NAND2TETRIS_VM_COMMAND_HPP
#define NAND2TETRIS_VM_COMMAND_HPP

#include "types.hpp"
#include <string>
#include <variant>

namespace n2t {

// ==============================================================================
// Arithmetic Commands
// ==============================================================================

/**
 * @brief Represents an arithmetic/logical VM command
 *
 * These commands operate on the stack:
 * - Binary: pop two values, compute, push result (add, sub, and, or, eq, gt, lt)
 * - Unary: pop one value, compute, push result (neg, not)
 *
 * Example VM code:
 *   push constant 7
 *   push constant 8
 *   add           // pops 8 and 7, pushes 15
 */
struct ArithmeticCommand {
    ArithmeticOp operation;  // Which operation (ADD, SUB, NEG, etc.)

    // For debugging: original source location
    LineNumber source_line;
};

// ==============================================================================
// Memory Access Commands (Push/Pop)
// ==============================================================================

/**
 * @brief Represents a push command: push segment index
 *
 * Pushes a value from a memory segment onto the stack.
 *
 * Example: "push local 2" pushes the value of local variable 2 onto the stack
 *
 * Memory segments and their meaning:
 * - local:    Function's local variables (LCL[index])
 * - argument: Function's arguments (ARG[index])
 * - this:     Current object's fields (THIS[index])
 * - that:     Array access (THAT[index])
 * - constant: Just the number itself (push constant 5 pushes 5)
 * - static:   Global variables shared within a .vm file
 * - temp:     Temporary storage (only 8 slots: temp 0-7)
 * - pointer:  Access THIS/THAT pointers (pointer 0 = THIS, pointer 1 = THAT)
 */
struct PushCommand {
    SegmentType segment;     // Which segment to read from
    uint16_t index;          // Index within the segment

    // For static segment: which file this belongs to (affects address)
    std::string file_name;

    // For debugging
    LineNumber source_line;
};

/**
 * @brief Represents a pop command: pop segment index
 *
 * Pops a value from the stack into a memory segment.
 *
 * Example: "pop local 2" pops the top of the stack into local variable 2
 *
 * Note: You cannot pop to the "constant" segment (constants are read-only)
 */
struct PopCommand {
    SegmentType segment;     // Which segment to write to
    uint16_t index;          // Index within the segment

    // For static segment
    std::string file_name;

    // For debugging
    LineNumber source_line;
};

// ==============================================================================
// Program Flow Commands
// ==============================================================================

/**
 * @brief Represents a label definition: label LABEL_NAME
 *
 * Labels mark positions in the code for jumps.
 * Labels are scoped to the current function.
 *
 * Example: "label LOOP" creates a label named LOOP at this position
 */
struct LabelCommand {
    std::string label_name;  // The label identifier

    // For debugging
    LineNumber source_line;
};

/**
 * @brief Represents an unconditional jump: goto LABEL_NAME
 *
 * Jumps to the specified label unconditionally.
 *
 * Example: "goto LOOP" jumps to the LOOP label
 */
struct GotoCommand {
    std::string label_name;  // Target label to jump to

    // For debugging
    LineNumber source_line;
};

/**
 * @brief Represents a conditional jump: if-goto LABEL_NAME
 *
 * Pops the top value from the stack:
 * - If the value is NOT zero (true), jumps to the label
 * - If the value is zero (false), continues to next command
 *
 * In VM, true is represented as -1 (all bits set) and false as 0.
 * But any non-zero value is treated as true for if-goto.
 *
 * Example:
 *   push local 0
 *   if-goto LOOP    // jumps to LOOP if local 0 is not zero
 */
struct IfGotoCommand {
    std::string label_name;  // Target label if condition is true

    // For debugging
    LineNumber source_line;
};

// ==============================================================================
// Function Commands
// ==============================================================================

/**
 * @brief Represents a function definition: function functionName nVars
 *
 * Declares a function and initializes its local variables to 0.
 *
 * Example: "function Math.multiply 2" defines a function with 2 local variables
 *
 * The function name follows the convention: ClassName.functionName
 */
struct FunctionCommand {
    std::string function_name;   // Full function name (e.g., "Main.main")
    uint16_t num_locals;         // Number of local variables

    // For debugging
    LineNumber source_line;
};

/**
 * @brief Represents a function call: call functionName nArgs
 *
 * Calls a function with nArgs arguments already pushed on the stack.
 *
 * Before the call:
 *   - Caller has pushed nArgs arguments onto the stack
 *
 * The call command:
 *   1. Pushes return address (address of next command)
 *   2. Pushes saved LCL, ARG, THIS, THAT
 *   3. Sets ARG = SP - nArgs - 5 (points to first argument)
 *   4. Sets LCL = SP (points to local segment)
 *   5. Jumps to function
 *
 * Example: "call Math.multiply 2" calls multiply with 2 arguments
 */
struct CallCommand {
    std::string function_name;   // Function to call
    uint16_t num_args;           // Number of arguments pushed by caller

    // For debugging
    LineNumber source_line;
};

/**
 * @brief Represents a return from function: return
 *
 * Returns from the current function to the caller.
 *
 * The return command:
 *   1. Gets return value from top of stack
 *   2. Restores SP, THIS, THAT, ARG, LCL to caller's values
 *   3. Places return value at ARG[0] (where caller expects it)
 *   4. Jumps to return address
 *
 * After return, the caller can access the return value at the top of its stack.
 */
struct ReturnCommand {
    // For debugging
    LineNumber source_line;
};

// ==============================================================================
// VMCommand - Unified Command Type
// ==============================================================================

/**
 * @brief A single VM command (any type)
 *
 * We use std::variant to represent any VM command in a type-safe way.
 * This allows the execution engine to handle all commands uniformly
 * while still knowing the exact type of each command.
 *
 * Usage:
 *   VMCommand cmd = parse_command(line);
 *   if (std::holds_alternative<PushCommand>(cmd)) {
 *       auto& push = std::get<PushCommand>(cmd);
 *       // handle push...
 *   }
 *
 * Or with std::visit:
 *   std::visit([](auto& cmd) {
 *       // handle cmd based on its type
 *   }, command);
 */
using VMCommand = std::variant<
    ArithmeticCommand,
    PushCommand,
    PopCommand,
    LabelCommand,
    GotoCommand,
    IfGotoCommand,
    FunctionCommand,
    CallCommand,
    ReturnCommand
>;

// ==============================================================================
// Helper Functions
// ==============================================================================

/**
 * @brief Get the source line number from any command
 *
 * Useful for error messages and debugging.
 */
LineNumber get_source_line(const VMCommand& cmd);

/**
 * @brief Convert a command to a human-readable string
 *
 * Useful for debugging and disassembly.
 *
 * Example: PushCommand{LOCAL, 2} -> "push local 2"
 */
std::string command_to_string(const VMCommand& cmd);

/**
 * @brief Check if a command is a branching command
 *
 * Branching commands can change the flow of execution:
 * - goto, if-goto, call, return
 */
bool is_branching_command(const VMCommand& cmd);

/**
 * @brief Check if a command modifies the stack
 *
 * All commands except label modify the stack or control flow.
 */
bool modifies_stack(const VMCommand& cmd);

}  // namespace n2t

#endif  // NAND2TETRIS_VM_COMMAND_HPP
