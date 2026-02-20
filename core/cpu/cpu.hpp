// ==============================================================================
// Hack CPU Execution Engine
// ==============================================================================
// Simulates the Hack CPU: fetches instructions from ROM, decodes and
// executes them, manages the A, D, and PC registers.
//
// The engine provides:
// - Program loading from .hack files
// - Execution control (run, step, pause, breakpoints)
// - Register and memory inspection
// - Disassembly of instructions
// - Execution statistics
//
// The fetch-decode-execute cycle is optimized for speed: inline bit
// extraction in the hot loop, no heap allocation per instruction.
// ==============================================================================

#ifndef NAND2TETRIS_CPU_HPP
#define NAND2TETRIS_CPU_HPP

#include "instruction.hpp"
#include "memory.hpp"
#include <unordered_set>
#include <vector>
#include <string>

namespace n2t {

// ==============================================================================
// CPU State
// ==============================================================================

enum class CPUState {
    READY,      // Program loaded, ready to run
    RUNNING,    // Currently executing
    PAUSED,     // Paused (breakpoint, step, or user request)
    HALTED,     // PC past end of program
    ERROR       // Runtime error occurred
};

enum class CPUPauseReason {
    NONE,
    STEP_COMPLETE,
    BREAKPOINT,
    USER_REQUEST
};

// ==============================================================================
// CPU Statistics
// ==============================================================================

struct CPUStats {
    uint64_t instructions_executed = 0;
    uint64_t a_instruction_count = 0;
    uint64_t c_instruction_count = 0;
    uint64_t jump_count = 0;       // Jumps actually taken
    uint64_t memory_reads = 0;     // M reads (a-bit=1)
    uint64_t memory_writes = 0;    // M writes (d3 dest bit)

    void reset() {
        instructions_executed = 0;
        a_instruction_count = 0;
        c_instruction_count = 0;
        jump_count = 0;
        memory_reads = 0;
        memory_writes = 0;
    }
};

// ==============================================================================
// CPU Engine Class
// ==============================================================================

/**
 * @brief The Hack CPU execution engine.
 *
 * Basic usage:
 *   CPUEngine cpu;
 *   cpu.load_file("Prog.hack");
 *   cpu.run();
 *
 * Debugging usage:
 *   CPUEngine cpu;
 *   cpu.load_file("Prog.hack");
 *   cpu.add_breakpoint(10);
 *   cpu.run();               // Runs until breakpoint
 *   cpu.step();              // Execute one instruction
 *   Word d = cpu.get_d();    // Inspect registers
 */
class CPUEngine {
public:
    CPUEngine();

    // =========================================================================
    // Program Loading
    // =========================================================================

    /**
     * @brief Load a .hack file into ROM.
     */
    void load_file(const std::string& file_path);

    /**
     * @brief Load from a string of binary lines.
     */
    void load_string(const std::string& hack_text);

    /**
     * @brief Load from pre-parsed instruction words.
     */
    void load(const std::vector<Word>& instructions);

    /**
     * @brief Reset CPU to initial state (registers zeroed, RAM cleared).
     */
    void reset();

    // =========================================================================
    // Execution Control
    // =========================================================================

    /**
     * @brief Run until halt, error, or breakpoint.
     */
    CPUState run();

    /**
     * @brief Run for at most max_instructions cycles.
     */
    CPUState run_for(uint64_t max_instructions);

    /**
     * @brief Execute a single instruction.
     */
    CPUState step();

    /**
     * @brief Request pause (can be called from another thread).
     */
    void pause();

    bool is_running() const { return state_ == CPUState::RUNNING; }
    CPUState get_state() const { return state_; }
    CPUPauseReason get_pause_reason() const { return pause_reason_; }

    // =========================================================================
    // Register Inspection
    // =========================================================================

    Word get_a() const { return a_register_; }
    Word get_d() const { return d_register_; }
    Address get_pc() const { return pc_; }

    // =========================================================================
    // Memory Access (delegated to CPUMemory)
    // =========================================================================

    Word read_ram(Address address) const { return memory_.read_ram(address); }
    void write_ram(Address address, Word value) { memory_.write_ram(address, value); }
    Word read_rom(Address address) const { return memory_.read_rom(address); }
    size_t rom_size() const { return memory_.rom_size(); }

    const CPUMemory& memory() const { return memory_; }
    CPUMemory& memory() { return memory_; }

    // =========================================================================
    // I/O (delegated to CPUMemory)
    // =========================================================================

    const Word* get_screen_buffer() const { return memory_.screen_buffer(); }
    Word get_keyboard() const { return memory_.get_keyboard(); }
    void set_keyboard(Word key_code) { memory_.set_keyboard(key_code); }

    // =========================================================================
    // Breakpoints
    // =========================================================================

    void add_breakpoint(Address rom_address);
    void remove_breakpoint(Address rom_address);
    void clear_breakpoints();
    bool has_breakpoint(Address rom_address) const;
    std::vector<Address> get_breakpoints() const;

    // =========================================================================
    // Disassembly
    // =========================================================================

    /**
     * @brief Get the decoded form of the current instruction.
     */
    DecodedInstruction get_current_instruction() const;

    /**
     * @brief Disassemble the instruction at a ROM address.
     */
    std::string disassemble(Address rom_address) const;

    /**
     * @brief Disassemble a range of ROM addresses [start, end).
     */
    std::vector<std::string> disassemble_range(Address start, Address end) const;

    // =========================================================================
    // Statistics and Error
    // =========================================================================

    const CPUStats& get_stats() const { return stats_; }
    const std::string& get_error_message() const { return error_message_; }
    Address get_error_location() const { return error_location_; }

private:
    // Registers
    Word a_register_ = 0;
    Word d_register_ = 0;
    Address pc_ = 0;

    // Memory
    CPUMemory memory_;

    // State
    CPUState state_ = CPUState::READY;
    CPUPauseReason pause_reason_ = CPUPauseReason::NONE;
    bool pause_requested_ = false;

    // Statistics
    CPUStats stats_;

    // Breakpoints
    std::unordered_set<Address> breakpoints_;

    // Error
    std::string error_message_;
    Address error_location_ = 0;

    // =========================================================================
    // Internal
    // =========================================================================

    /**
     * @brief Execute one instruction. Returns true to continue.
     */
    bool execute_instruction();

    /**
     * @brief Evaluate the ALU for a computation code.
     */
    Word compute_alu(Computation comp, Word d_val, Word am_val) const;

    /**
     * @brief Evaluate whether a jump should be taken.
     */
    bool should_jump(JumpCondition jump, Word alu_output) const;

    void set_error(const std::string& message);
};

}  // namespace n2t

#endif  // NAND2TETRIS_CPU_HPP
