// ==============================================================================
// Common Type Definitions
// ==============================================================================
// This file defines basic types used throughout the Nand2Tetris suite.
// Using explicit types makes the code more readable and helps catch bugs.
// ==============================================================================

#ifndef NAND2TETRIS_COMMON_TYPES_HPP
#define NAND2TETRIS_COMMON_TYPES_HPP

#include <cstdint>      // For fixed-width integer types
#include <string>       // For std::string
#include <vector>       // For std::vector
#include <optional>     // For std::optional (values that might not exist)
#include <memory>       // For smart pointers

namespace n2t {  // n2t = nand2tetris namespace

// ==============================================================================
// Hack Computer Basic Types
// ==============================================================================

/**
 * @brief A 16-bit word in the Hack computer
 *
 * The Hack computer is a 16-bit architecture, meaning:
 * - All memory addresses are 16 bits
 * - All instructions are 16 bits
 * - All data registers are 16 bits
 *
 * We use uint16_t to ensure it's exactly 16 bits on all platforms.
 */
using Word = uint16_t;

/**
 * @brief A 15-bit memory address
 *
 * Hack computer has 32K (32,768) words of RAM addressed with 15 bits.
 * We still store it as 16-bit for convenience, but only use lower 15 bits.
 * Valid range: 0 to 32,767
 */
using Address = uint16_t;

/**
 * @brief An 8-bit byte
 *
 * Used for representing characters and smaller data units.
 */
using Byte = uint8_t;

// ==============================================================================
// VM Types
// ==============================================================================

/**
 * @brief VM memory segment types
 *
 * The VM has 8 memory segments that hold different kinds of data:
 * - LOCAL: Function local variables
 * - ARGUMENT: Function arguments
 * - THIS: Current object's fields (pointer to heap)
 * - THAT: Array elements (pointer to heap)
 * - CONSTANT: Read-only constants (0-32767)
 * - STATIC: Global variables (one per .vm file)
 * - TEMP: Temporary variables (only 8 of them: temp 0-7)
 * - POINTER: Access to THIS/THAT pointers (pointer 0 = THIS, pointer 1 = THAT)
 */
enum class SegmentType {
    LOCAL,
    ARGUMENT,
    THIS,
    THAT,
    CONSTANT,
    STATIC,
    TEMP,
    POINTER
};

/**
 * @brief VM command types
 *
 * All VM commands fall into one of these categories:
 * - ARITHMETIC: add, sub, neg, eq, gt, lt, and, or, not
 * - PUSH: Push value onto stack
 * - POP: Pop value from stack
 * - LABEL: Define a label for branching
 * - GOTO: Unconditional jump to label
 * - IF_GOTO: Conditional jump (pop stack, jump if true)
 * - FUNCTION: Function definition
 * - CALL: Call a function
 * - RETURN: Return from function
 */
enum class CommandType {
    ARITHMETIC,
    PUSH,
    POP,
    LABEL,
    GOTO,
    IF_GOTO,
    FUNCTION,
    CALL,
    RETURN
};

/**
 * @brief Arithmetic/logical operations in the VM
 *
 * These operations work on the stack:
 * - Binary operations (ADD, SUB, etc.): pop two values, push result
 * - Unary operations (NEG, NOT): pop one value, push result
 * - Comparison operations (EQ, GT, LT): pop two values, push -1 (true) or 0 (false)
 */
enum class ArithmeticOp {
    ADD,   // x + y
    SUB,   // x - y (note: order matters! Second value - top value)
    NEG,   // -y (unary)
    EQ,    // x == y (result: -1 if true, 0 if false)
    GT,    // x > y  (result: -1 if true, 0 if false)
    LT,    // x < y  (result: -1 if true, 0 if false)
    AND,   // x & y (bitwise AND)
    OR,    // x | y (bitwise OR)
    NOT    // ~y (bitwise NOT, unary)
};

// ==============================================================================
// CPU Types
// ==============================================================================

/**
 * @brief CPU instruction type
 *
 * Hack CPU has two instruction formats:
 * - A_INSTRUCTION: Address instruction (format: 0vvvvvvvvvvvvvvv)
 *   Sets the A register to a 15-bit value
 *
 * - C_INSTRUCTION: Compute instruction (format: 111accccccdddjjj)
 *   a = ALU input selector (0 = A register, 1 = Memory[A])
 *   cccccc = ALU computation (what operation to perform)
 *   ddd = destination (where to store result: A, D, M, or combinations)
 *   jjj = jump condition (when to jump to address in A)
 */
enum class InstructionType {
    A_INSTRUCTION,  // Address instruction
    C_INSTRUCTION   // Compute instruction
};

// ==============================================================================
// HDL Types
// ==============================================================================

/**
 * @brief Logic signal value
 *
 * In digital logic, each wire carries one of two values:
 * - LOW (false, 0)
 * - HIGH (true, 1)
 */
enum class Signal {
    LOW = 0,
    HIGH = 1
};

/**
 * @brief Multi-bit bus value
 *
 * A bus is a collection of wires (bits) treated as a single unit.
 * For example, a 16-bit bus represents a 16-bit number.
 *
 * We use vector<Signal> rather than an integer so we can see each bit clearly
 * during debugging and visualization.
 */
using Bus = std::vector<Signal>;

// ==============================================================================
// Utility Type Aliases
// ==============================================================================

/**
 * @brief Source code line number
 *
 * Used for mapping between different representations (Jack -> VM -> ASM -> Hack)
 * and for displaying error messages with line numbers.
 */
using LineNumber = size_t;

/**
 * @brief Symbol name (variable, label, function name, etc.)
 *
 * In assembly and VM code, we use symbolic names for readability:
 * - Variables: sum, i, count
 * - Labels: LOOP, END, SKIP
 * - Functions: Main.main, Math.multiply
 */
using Symbol = std::string;

/**
 * @brief File path
 *
 * Used for loading source files and reporting errors with file context.
 */
using FilePath = std::string;

// ==============================================================================
// Helper Functions
// ==============================================================================

/**
 * @brief Convert a Signal to bool
 *
 * Makes it easy to use Signal values in conditions:
 * if (to_bool(signal)) { ... }
 */
inline bool to_bool(Signal s) {
    return s == Signal::HIGH;
}

/**
 * @brief Convert bool to Signal
 *
 * Makes it easy to create Signal values from bool:
 * Signal s = to_signal(x > 5);
 */
inline Signal to_signal(bool b) {
    return b ? Signal::HIGH : Signal::LOW;
}

/**
 * @brief Convert Bus to Word (16-bit unsigned integer)
 *
 * Converts a 16-bit bus to its numeric value.
 * The bus is interpreted as a binary number with bit 0 (MSB) at the left.
 *
 * Example: Bus{HIGH, LOW, HIGH} with size 3 = binary 101 = decimal 5
 *
 * @param bus The bus to convert (must be 16 bits or less)
 * @return The numeric value
 */
Word bus_to_word(const Bus& bus);

/**
 * @brief Convert Word to Bus (16-bit bus)
 *
 * Converts a 16-bit number to a bus representation.
 *
 * @param word The value to convert
 * @param width How many bits (default 16)
 * @return A bus with the binary representation
 */
Bus word_to_bus(Word word, size_t width = 16);

/**
 * @brief Convert SegmentType to string for debugging
 *
 * Makes it easy to print segment names in error messages and logs.
 *
 * @param segment The segment type
 * @return Human-readable name (e.g., "local", "argument")
 */
const char* segment_to_string(SegmentType segment);

/**
 * @brief Convert ArithmeticOp to string for debugging
 *
 * @param op The arithmetic operation
 * @return Human-readable name (e.g., "add", "sub", "eq")
 */
const char* arithmetic_op_to_string(ArithmeticOp op);

}  // namespace n2t

#endif  // NAND2TETRIS_COMMON_TYPES_HPP
