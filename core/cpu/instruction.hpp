// ==============================================================================
// Hack CPU Instruction Decoder
// ==============================================================================
// Decodes 16-bit Hack machine instructions into structured representations.
//
// Two instruction formats:
//   A-instruction: 0vvvvvvvvvvvvvvv  (sets A register to 15-bit value)
//   C-instruction: 111accccccdddjjj  (compute, store, jump)
//
// The decoder is used in two contexts:
//   1. Load-time validation (decode_instruction_checked)
//   2. Debug/disassembly (instruction_to_string)
//
// The hot execution loop in CPUEngine does inline bit extraction for
// performance rather than going through the DecodedInstruction struct.
// ==============================================================================

#ifndef NAND2TETRIS_CPU_INSTRUCTION_HPP
#define NAND2TETRIS_CPU_INSTRUCTION_HPP

#include "types.hpp"
#include "error.hpp"
#include <string>

namespace n2t {

// ==============================================================================
// ALU Computation Codes
// ==============================================================================

/**
 * @brief All 28 valid ALU computations from the Hack specification.
 *
 * Values encode the 7-bit {a, c1-c6} field directly from bits [12:6]
 * of a C-instruction, so decoding is a direct cast + validation.
 *
 * When a=0, the ALU uses the A register as input.
 * When a=1, the ALU uses M (RAM[A]) as input.
 */
enum class Computation : uint8_t {
    // a=0 computations (use A register)
    ZERO        = 0b0101010,  // 0
    ONE         = 0b0111111,  // 1
    NEG_ONE     = 0b0111010,  // -1
    D           = 0b0001100,  // D
    A           = 0b0110000,  // A
    NOT_D       = 0b0001101,  // !D
    NOT_A       = 0b0110001,  // !A
    NEG_D       = 0b0001111,  // -D
    NEG_A       = 0b0110011,  // -A
    D_PLUS_1    = 0b0011111,  // D+1
    A_PLUS_1    = 0b0110111,  // A+1
    D_MINUS_1   = 0b0001110,  // D-1
    A_MINUS_1   = 0b0110010,  // A-1
    D_PLUS_A    = 0b0000010,  // D+A
    D_MINUS_A   = 0b0010011,  // D-A
    A_MINUS_D   = 0b0000111,  // A-D
    D_AND_A     = 0b0000000,  // D&A
    D_OR_A      = 0b0010101,  // D|A

    // a=1 computations (use M = RAM[A])
    M           = 0b1110000,  // M
    NOT_M       = 0b1110001,  // !M
    NEG_M       = 0b1110011,  // -M
    M_PLUS_1    = 0b1110111,  // M+1
    M_MINUS_1   = 0b1110010,  // M-1
    D_PLUS_M    = 0b1000010,  // D+M
    D_MINUS_M   = 0b1010011,  // D-M
    M_MINUS_D   = 0b1000111,  // M-D
    D_AND_M     = 0b1000000,  // D&M
    D_OR_M      = 0b1010101,  // D|M
};

// ==============================================================================
// Destination Flags
// ==============================================================================

/**
 * @brief Where to store the ALU result.
 *
 * Extracted from the ddd bits (bits [5:3]) of a C-instruction.
 * Multiple destinations can be active simultaneously.
 */
struct Destination {
    bool a = false;  // d1 (bit 5): store in A register
    bool d = false;  // d2 (bit 4): store in D register
    bool m = false;  // d3 (bit 3): store in RAM[A]
};

// ==============================================================================
// Jump Condition
// ==============================================================================

/**
 * @brief When to jump to the address in the A register.
 *
 * Extracted from the jjj bits (bits [2:0]) of a C-instruction.
 * The condition is evaluated on the ALU output (as signed 16-bit).
 *   j1 (bit 2) = jump if output < 0
 *   j2 (bit 1) = jump if output == 0
 *   j3 (bit 0) = jump if output > 0
 */
enum class JumpCondition : uint8_t {
    NO_JUMP = 0b000,  // Never jump
    JGT     = 0b001,  // Jump if out > 0
    JEQ     = 0b010,  // Jump if out == 0
    JGE     = 0b011,  // Jump if out >= 0
    JLT     = 0b100,  // Jump if out < 0
    JNE     = 0b101,  // Jump if out != 0
    JLE     = 0b110,  // Jump if out <= 0
    JMP     = 0b111,  // Always jump
};

// ==============================================================================
// Decoded Instruction
// ==============================================================================

/**
 * @brief A decoded Hack instruction.
 *
 * Used for debugging, disassembly, and load-time validation.
 * The hot execution loop does inline bit extraction instead.
 */
struct DecodedInstruction {
    InstructionType type;

    // A-instruction: the 15-bit immediate value
    Word value;

    // C-instruction fields
    Computation comp;
    Destination dest;
    JumpCondition jump;
    bool reads_memory;  // true if comp uses M (a-bit == 1)
};

// ==============================================================================
// Decoding Functions
// ==============================================================================

/**
 * @brief Decode a 16-bit instruction word.
 *
 * Fast path — no validation of computation codes.
 */
DecodedInstruction decode_instruction(Word instruction);

/**
 * @brief Decode with validation.
 *
 * Throws ParseError for invalid computation codes.
 * Use at load time, not in the execution hot loop.
 */
DecodedInstruction decode_instruction_checked(Word instruction);

/**
 * @brief Check if a 7-bit computation code is valid.
 */
bool is_valid_computation(uint8_t comp_bits);

/**
 * @brief Convert a decoded instruction to human-readable assembly.
 *
 * Examples: "@42", "D=A+1", "M=D", "0;JMP", "D=M+1;JGT"
 */
std::string instruction_to_string(const DecodedInstruction& instr);

/**
 * @brief Decode and disassemble a raw 16-bit instruction.
 */
std::string instruction_to_string(Word raw_instruction);

}  // namespace n2t

#endif  // NAND2TETRIS_CPU_INSTRUCTION_HPP
