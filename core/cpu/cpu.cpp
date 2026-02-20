// ==============================================================================
// Hack CPU Execution Engine Implementation
// ==============================================================================

#include "cpu.hpp"
#include <algorithm>

namespace n2t {

// ==============================================================================
// Constructor
// ==============================================================================

CPUEngine::CPUEngine() = default;

// ==============================================================================
// Program Loading
// ==============================================================================

void CPUEngine::load_file(const std::string& file_path) {
    memory_.load_rom_file(file_path);
    state_ = CPUState::READY;
    pc_ = 0;
    a_register_ = 0;
    d_register_ = 0;
    stats_.reset();
}

void CPUEngine::load_string(const std::string& hack_text) {
    memory_.load_rom_string(hack_text);
    state_ = CPUState::READY;
    pc_ = 0;
    a_register_ = 0;
    d_register_ = 0;
    stats_.reset();
}

void CPUEngine::load(const std::vector<Word>& instructions) {
    memory_.load_rom(instructions);
    state_ = CPUState::READY;
    pc_ = 0;
    a_register_ = 0;
    d_register_ = 0;
    stats_.reset();
}

void CPUEngine::reset() {
    memory_.reset();
    a_register_ = 0;
    d_register_ = 0;
    pc_ = 0;
    state_ = CPUState::READY;
    pause_reason_ = CPUPauseReason::NONE;
    pause_requested_ = false;
    stats_.reset();
    error_message_.clear();
    error_location_ = 0;
}

// ==============================================================================
// Execution Control
// ==============================================================================

CPUState CPUEngine::run() {
    if (state_ == CPUState::READY || state_ == CPUState::PAUSED) {
        state_ = CPUState::RUNNING;
        pause_reason_ = CPUPauseReason::NONE;
        pause_requested_ = false;

        while (state_ == CPUState::RUNNING) {
            if (!execute_instruction()) {
                break;
            }
        }
    }

    return state_;
}

CPUState CPUEngine::run_for(uint64_t max_instructions) {
    if (state_ == CPUState::READY || state_ == CPUState::PAUSED) {
        state_ = CPUState::RUNNING;
        pause_reason_ = CPUPauseReason::NONE;
        pause_requested_ = false;

        uint64_t count = 0;
        while (state_ == CPUState::RUNNING && count < max_instructions) {
            if (!execute_instruction()) {
                break;
            }
            count++;
        }

        if (state_ == CPUState::RUNNING) {
            state_ = CPUState::PAUSED;
            pause_reason_ = CPUPauseReason::USER_REQUEST;
        }
    }

    return state_;
}

CPUState CPUEngine::step() {
    if (state_ == CPUState::READY || state_ == CPUState::PAUSED) {
        state_ = CPUState::RUNNING;
        pause_reason_ = CPUPauseReason::NONE;

        execute_instruction();

        if (state_ == CPUState::RUNNING) {
            state_ = CPUState::PAUSED;
            pause_reason_ = CPUPauseReason::STEP_COMPLETE;
        }
    }

    return state_;
}

void CPUEngine::pause() {
    pause_requested_ = true;
}

// ==============================================================================
// Breakpoints
// ==============================================================================

void CPUEngine::add_breakpoint(Address rom_address) {
    breakpoints_.insert(rom_address);
}

void CPUEngine::remove_breakpoint(Address rom_address) {
    breakpoints_.erase(rom_address);
}

void CPUEngine::clear_breakpoints() {
    breakpoints_.clear();
}

bool CPUEngine::has_breakpoint(Address rom_address) const {
    return breakpoints_.count(rom_address) > 0;
}

std::vector<Address> CPUEngine::get_breakpoints() const {
    return std::vector<Address>(breakpoints_.begin(), breakpoints_.end());
}

// ==============================================================================
// Disassembly
// ==============================================================================

DecodedInstruction CPUEngine::get_current_instruction() const {
    if (pc_ >= memory_.rom_size()) {
        DecodedInstruction empty{};
        empty.type = InstructionType::A_INSTRUCTION;
        empty.value = 0;
        return empty;
    }
    return decode_instruction(memory_.read_rom(pc_));
}

std::string CPUEngine::disassemble(Address rom_address) const {
    return instruction_to_string(memory_.read_rom(rom_address));
}

std::vector<std::string> CPUEngine::disassemble_range(Address start, Address end) const {
    std::vector<std::string> result;
    for (Address addr = start; addr < end && addr < memory_.rom_size(); addr++) {
        result.push_back(instruction_to_string(memory_.read_rom(addr)));
    }
    return result;
}

// ==============================================================================
// Execution Core
// ==============================================================================

bool CPUEngine::execute_instruction() {
    // Check halt: PC past loaded program
    if (pc_ >= memory_.rom_size()) {
        state_ = CPUState::HALTED;
        return false;
    }

    // Check pause request
    if (pause_requested_) {
        pause_requested_ = false;
        state_ = CPUState::PAUSED;
        pause_reason_ = CPUPauseReason::USER_REQUEST;
        return false;
    }

    // Check breakpoints (skip on first instruction after run to avoid
    // re-triggering on the same breakpoint)
    if (stats_.instructions_executed > 0 && breakpoints_.count(pc_)) {
        state_ = CPUState::PAUSED;
        pause_reason_ = CPUPauseReason::BREAKPOINT;
        return false;
    }

    try {
        // Fetch
        Word raw = memory_.rom_ptr()[pc_];

        if (!(raw & 0x8000)) {
            // ---- A-instruction ----
            a_register_ = raw & 0x7FFF;
            pc_++;
            stats_.a_instruction_count++;

        } else {
            // ---- C-instruction: 111accccccdddjjj ----
            uint8_t comp_bits = static_cast<uint8_t>((raw >> 6) & 0x7F);
            uint8_t dest_bits = static_cast<uint8_t>((raw >> 3) & 0x7);
            uint8_t jump_bits = static_cast<uint8_t>(raw & 0x7);

            // Determine ALU input: A register or M (RAM[A])
            Word am_val;
            if (comp_bits & 0x40) {
                // a-bit set: use M = RAM[A]
                am_val = memory_.read_ram(a_register_);
                stats_.memory_reads++;
            } else {
                am_val = a_register_;
            }

            // Compute ALU result
            Word alu_output = compute_alu(
                static_cast<Computation>(comp_bits), d_register_, am_val);

            // Store results — save original A for M write
            Word original_a = a_register_;

            if (dest_bits & 0x4) {  // d1: A register
                a_register_ = alu_output;
            }
            if (dest_bits & 0x2) {  // d2: D register
                d_register_ = alu_output;
            }
            if (dest_bits & 0x1) {  // d3: M (RAM[A])
                memory_.write_ram(original_a, alu_output);
                stats_.memory_writes++;
            }

            // Jump evaluation
            if (should_jump(static_cast<JumpCondition>(jump_bits), alu_output)) {
                pc_ = a_register_;
                stats_.jump_count++;
            } else {
                pc_++;
            }

            stats_.c_instruction_count++;
        }

        stats_.instructions_executed++;

        // Check if PC has reached end of program after execution
        if (pc_ >= memory_.rom_size()) {
            state_ = CPUState::HALTED;
            return false;
        }

    } catch (const N2TError& e) {
        error_message_ = e.what();
        error_location_ = pc_;
        state_ = CPUState::ERROR;
        return false;
    } catch (const std::exception& e) {
        error_message_ = std::string("Unexpected error: ") + e.what();
        error_location_ = pc_;
        state_ = CPUState::ERROR;
        return false;
    }

    return true;
}

// ==============================================================================
// ALU
// ==============================================================================

Word CPUEngine::compute_alu(Computation comp, Word d_val, Word am_val) const {
    int16_t d = static_cast<int16_t>(d_val);
    int16_t am = static_cast<int16_t>(am_val);
    int16_t result;

    switch (comp) {
        case Computation::ZERO:      result = 0; break;
        case Computation::ONE:       result = 1; break;
        case Computation::NEG_ONE:   result = -1; break;
        case Computation::D:         result = d; break;
        case Computation::A:
        case Computation::M:         result = am; break;
        case Computation::NOT_D:     result = static_cast<int16_t>(~d); break;
        case Computation::NOT_A:
        case Computation::NOT_M:     result = static_cast<int16_t>(~am); break;
        case Computation::NEG_D:     result = static_cast<int16_t>(-d); break;
        case Computation::NEG_A:
        case Computation::NEG_M:     result = static_cast<int16_t>(-am); break;
        case Computation::D_PLUS_1:  result = static_cast<int16_t>(d + 1); break;
        case Computation::A_PLUS_1:
        case Computation::M_PLUS_1:  result = static_cast<int16_t>(am + 1); break;
        case Computation::D_MINUS_1: result = static_cast<int16_t>(d - 1); break;
        case Computation::A_MINUS_1:
        case Computation::M_MINUS_1: result = static_cast<int16_t>(am - 1); break;
        case Computation::D_PLUS_A:
        case Computation::D_PLUS_M:  result = static_cast<int16_t>(d + am); break;
        case Computation::D_MINUS_A:
        case Computation::D_MINUS_M: result = static_cast<int16_t>(d - am); break;
        case Computation::A_MINUS_D:
        case Computation::M_MINUS_D: result = static_cast<int16_t>(am - d); break;
        case Computation::D_AND_A:
        case Computation::D_AND_M:   result = d & am; break;
        case Computation::D_OR_A:
        case Computation::D_OR_M:    result = d | am; break;
        default:
            throw RuntimeError(
                "Invalid ALU computation code at ROM[" + std::to_string(pc_) +
                "]. The instruction may be corrupted.");
    }

    return static_cast<Word>(result);
}

// ==============================================================================
// Jump Evaluation
// ==============================================================================

bool CPUEngine::should_jump(JumpCondition jump, Word alu_output) const {
    if (jump == JumpCondition::NO_JUMP) return false;
    if (jump == JumpCondition::JMP) return true;

    int16_t val = static_cast<int16_t>(alu_output);
    uint8_t j = static_cast<uint8_t>(jump);

    // j1 (bit 2) = lt, j2 (bit 1) = eq, j3 (bit 0) = gt
    return (val < 0  && (j & 0x4)) ||
           (val == 0 && (j & 0x2)) ||
           (val > 0  && (j & 0x1));
}

void CPUEngine::set_error(const std::string& message) {
    error_message_ = message;
    error_location_ = pc_;
    state_ = CPUState::ERROR;
}

}  // namespace n2t
