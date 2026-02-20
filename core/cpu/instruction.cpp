// ==============================================================================
// Hack CPU Instruction Decoder Implementation
// ==============================================================================

#include "instruction.hpp"
#include <array>

namespace n2t {

// ==============================================================================
// Validation Lookup Table
// ==============================================================================

// Static lookup table: valid_comp[i] is true if i is a valid 7-bit comp code.
// Only 28 of the 128 possible values are valid.
static constexpr std::array<bool, 128> make_valid_comp_table() {
    std::array<bool, 128> table{};
    // a=0 computations
    table[0b0101010] = true;  // 0
    table[0b0111111] = true;  // 1
    table[0b0111010] = true;  // -1
    table[0b0001100] = true;  // D
    table[0b0110000] = true;  // A
    table[0b0001101] = true;  // !D
    table[0b0110001] = true;  // !A
    table[0b0001111] = true;  // -D
    table[0b0110011] = true;  // -A
    table[0b0011111] = true;  // D+1
    table[0b0110111] = true;  // A+1
    table[0b0001110] = true;  // D-1
    table[0b0110010] = true;  // A-1
    table[0b0000010] = true;  // D+A
    table[0b0010011] = true;  // D-A
    table[0b0000111] = true;  // A-D
    table[0b0000000] = true;  // D&A
    table[0b0010101] = true;  // D|A
    // a=1 computations
    table[0b1110000] = true;  // M
    table[0b1110001] = true;  // !M
    table[0b1110011] = true;  // -M
    table[0b1110111] = true;  // M+1
    table[0b1110010] = true;  // M-1
    table[0b1000010] = true;  // D+M
    table[0b1010011] = true;  // D-M
    table[0b1000111] = true;  // M-D
    table[0b1000000] = true;  // D&M
    table[0b1010101] = true;  // D|M
    return table;
}

static constexpr auto VALID_COMP = make_valid_comp_table();

// ==============================================================================
// Computation to String Table
// ==============================================================================

static const char* computation_to_string(Computation comp) {
    switch (comp) {
        case Computation::ZERO:      return "0";
        case Computation::ONE:       return "1";
        case Computation::NEG_ONE:   return "-1";
        case Computation::D:         return "D";
        case Computation::A:         return "A";
        case Computation::NOT_D:     return "!D";
        case Computation::NOT_A:     return "!A";
        case Computation::NEG_D:     return "-D";
        case Computation::NEG_A:     return "-A";
        case Computation::D_PLUS_1:  return "D+1";
        case Computation::A_PLUS_1:  return "A+1";
        case Computation::D_MINUS_1: return "D-1";
        case Computation::A_MINUS_1: return "A-1";
        case Computation::D_PLUS_A:  return "D+A";
        case Computation::D_MINUS_A: return "D-A";
        case Computation::A_MINUS_D: return "A-D";
        case Computation::D_AND_A:   return "D&A";
        case Computation::D_OR_A:    return "D|A";
        case Computation::M:         return "M";
        case Computation::NOT_M:     return "!M";
        case Computation::NEG_M:     return "-M";
        case Computation::M_PLUS_1:  return "M+1";
        case Computation::M_MINUS_1: return "M-1";
        case Computation::D_PLUS_M:  return "D+M";
        case Computation::D_MINUS_M: return "D-M";
        case Computation::M_MINUS_D: return "M-D";
        case Computation::D_AND_M:   return "D&M";
        case Computation::D_OR_M:    return "D|M";
        default:                     return "???";
    }
}

static const char* jump_to_string(JumpCondition jump) {
    switch (jump) {
        case JumpCondition::NO_JUMP: return "";
        case JumpCondition::JGT:     return "JGT";
        case JumpCondition::JEQ:     return "JEQ";
        case JumpCondition::JGE:     return "JGE";
        case JumpCondition::JLT:     return "JLT";
        case JumpCondition::JNE:     return "JNE";
        case JumpCondition::JLE:     return "JLE";
        case JumpCondition::JMP:     return "JMP";
        default:                     return "???";
    }
}

// ==============================================================================
// Decode Functions
// ==============================================================================

DecodedInstruction decode_instruction(Word instruction) {
    DecodedInstruction decoded{};

    if (!(instruction & 0x8000)) {
        // A-instruction: bit 15 == 0
        decoded.type = InstructionType::A_INSTRUCTION;
        decoded.value = instruction & 0x7FFF;
        decoded.reads_memory = false;
    } else {
        // C-instruction: bits 15-13 == 111
        decoded.type = InstructionType::C_INSTRUCTION;
        decoded.value = 0;

        uint8_t comp_bits = static_cast<uint8_t>((instruction >> 6) & 0x7F);
        uint8_t dest_bits = static_cast<uint8_t>((instruction >> 3) & 0x7);
        uint8_t jump_bits = static_cast<uint8_t>(instruction & 0x7);

        decoded.comp = static_cast<Computation>(comp_bits);
        decoded.dest.a = (dest_bits & 0x4) != 0;  // d1 bit
        decoded.dest.d = (dest_bits & 0x2) != 0;  // d2 bit
        decoded.dest.m = (dest_bits & 0x1) != 0;  // d3 bit
        decoded.jump = static_cast<JumpCondition>(jump_bits);
        decoded.reads_memory = (comp_bits & 0x40) != 0;  // a-bit
    }

    return decoded;
}

DecodedInstruction decode_instruction_checked(Word instruction) {
    DecodedInstruction decoded = decode_instruction(instruction);

    if (decoded.type == InstructionType::C_INSTRUCTION) {
        uint8_t comp_bits = static_cast<uint8_t>(decoded.comp);
        if (!is_valid_computation(comp_bits)) {
            throw ParseError("<rom>", 0,
                "Invalid ALU computation code 0b" +
                std::to_string((comp_bits >> 6) & 1) +
                std::to_string((comp_bits >> 5) & 1) +
                std::to_string((comp_bits >> 4) & 1) +
                std::to_string((comp_bits >> 3) & 1) +
                std::to_string((comp_bits >> 2) & 1) +
                std::to_string((comp_bits >> 1) & 1) +
                std::to_string(comp_bits & 1) +
                " in instruction word " + std::to_string(instruction) +
                ". This is not a valid Hack C-instruction computation.");
        }
    }

    return decoded;
}

bool is_valid_computation(uint8_t comp_bits) {
    if (comp_bits >= 128) return false;
    return VALID_COMP[comp_bits];
}

// ==============================================================================
// Disassembly
// ==============================================================================

std::string instruction_to_string(const DecodedInstruction& instr) {
    if (instr.type == InstructionType::A_INSTRUCTION) {
        return "@" + std::to_string(instr.value);
    }

    // C-instruction: [dest=]comp[;jump]
    std::string result;

    // Destination part
    if (instr.dest.a || instr.dest.d || instr.dest.m) {
        if (instr.dest.a) result += 'A';
        if (instr.dest.d) result += 'D';
        if (instr.dest.m) result += 'M';
        result += '=';
    }

    // Computation part
    result += computation_to_string(instr.comp);

    // Jump part
    if (instr.jump != JumpCondition::NO_JUMP) {
        result += ';';
        result += jump_to_string(instr.jump);
    }

    return result;
}

std::string instruction_to_string(Word raw_instruction) {
    return instruction_to_string(decode_instruction(raw_instruction));
}

}  // namespace n2t
