// ==============================================================================
// VM Execution Engine
// ==============================================================================
// The VM engine is the heart of the emulator. It:
// - Loads and executes VM programs
// - Manages the program counter
// - Dispatches VM commands to the appropriate handlers
// - Provides debugging hooks (step, breakpoints, state inspection)
//
// Performance is critical here - Jack programs may execute millions of
// VM commands, especially graphics/games that need real-time performance.
// ==============================================================================

#ifndef NAND2TETRIS_VM_ENGINE_HPP
#define NAND2TETRIS_VM_ENGINE_HPP

#include "vm_command.hpp"
#include "vm_parser.hpp"
#include "vm_memory.hpp"
#include <functional>
#include <unordered_set>

namespace n2t {

// ==============================================================================
// Execution State
// ==============================================================================

/**
 * @brief Current state of the VM engine
 */
enum class VMState {
    READY,      // Program loaded, ready to run
    RUNNING,    // Currently executing
    PAUSED,     // Paused (hit breakpoint or step completed)
    HALTED,     // Program finished normally
    ERROR       // Runtime error occurred
};

/**
 * @brief Reason for pausing execution
 */
enum class PauseReason {
    NONE,           // Not paused
    STEP_COMPLETE,  // Single step finished
    BREAKPOINT,     // Hit a breakpoint
    FUNCTION_ENTRY, // Entered a watched function
    FUNCTION_EXIT,  // Exited a watched function
    USER_REQUEST    // User requested pause
};

// ==============================================================================
// Execution Statistics
// ==============================================================================

/**
 * @brief Execution statistics for profiling
 */
struct VMStats {
    uint64_t instructions_executed = 0;  // Total VM commands executed
    uint64_t push_count = 0;             // Number of push commands
    uint64_t pop_count = 0;              // Number of pop commands
    uint64_t arithmetic_count = 0;       // Number of arithmetic commands
    uint64_t call_count = 0;             // Number of function calls
    uint64_t return_count = 0;           // Number of returns

    void reset() {
        instructions_executed = 0;
        push_count = 0;
        pop_count = 0;
        arithmetic_count = 0;
        call_count = 0;
        return_count = 0;
    }
};

// ==============================================================================
// VM Engine Class
// ==============================================================================

/**
 * @brief The VM execution engine
 *
 * This class executes VM programs and provides debugging facilities.
 *
 * Basic usage:
 *   VMEngine vm;
 *   vm.load_file("Main.vm");
 *   vm.run();  // Runs until halt or error
 *
 * Debugging usage:
 *   VMEngine vm;
 *   vm.load_file("Main.vm");
 *   vm.add_breakpoint(10);  // Break at command 10
 *   vm.run();               // Runs until breakpoint
 *   vm.step();              // Execute one command
 *   auto stack = vm.get_stack();  // Inspect state
 */
class VMEngine {
public:
    VMEngine();

    // =========================================================================
    // Program Loading
    // =========================================================================

    /**
     * @brief Load a VM program from a file
     *
     * @param file_path Path to .vm file
     * @throws FileError if file cannot be read
     * @throws ParseError if file is invalid
     */
    void load_file(const std::string& file_path);

    /**
     * @brief Load VM program from a string
     *
     * @param source VM source code
     * @param file_name Name for error messages
     */
    void load_string(const std::string& source,
                     const std::string& file_name = "<string>");

    /**
     * @brief Load all .vm files from a directory
     *
     * @param directory_path Path to directory
     */
    void load_directory(const std::string& directory_path);

    /**
     * @brief Set the entry point function
     *
     * By default, execution starts at Sys.init if present, then Main.main.
     *
     * @param function_name Function to start execution from
     */
    void set_entry_point(const std::string& function_name);

    /**
     * @brief Reset the engine to initial state
     *
     * Clears memory, resets PC to entry point.
     */
    void reset();

    // =========================================================================
    // Execution Control
    // =========================================================================

    /**
     * @brief Run until halt, error, or breakpoint
     *
     * @return Final state (HALTED, ERROR, or PAUSED)
     */
    VMState run();

    /**
     * @brief Run for a maximum number of instructions
     *
     * Useful for preventing infinite loops during testing.
     *
     * @param max_instructions Maximum instructions to execute
     * @return Final state
     */
    VMState run_for(uint64_t max_instructions);

    /**
     * @brief Execute a single VM command
     *
     * @return New state after execution
     */
    VMState step();

    /**
     * @brief Execute until the current function returns
     *
     * Useful for "step out" debugging.
     *
     * @return New state after execution
     */
    VMState step_out();

    /**
     * @brief Execute until entering a different function
     *
     * Useful for "step over" debugging (skips function calls).
     *
     * @return New state after execution
     */
    VMState step_over();

    /**
     * @brief Pause execution (can be called from another thread)
     */
    void pause();

    /**
     * @brief Check if the engine is currently running
     */
    bool is_running() const { return state_ == VMState::RUNNING; }

    /**
     * @brief Get current execution state
     */
    VMState get_state() const { return state_; }

    /**
     * @brief Get reason for last pause
     */
    PauseReason get_pause_reason() const { return pause_reason_; }

    // =========================================================================
    // State Inspection
    // =========================================================================

    /**
     * @brief Get current program counter
     *
     * Points to the next command to be executed.
     */
    size_t get_pc() const { return pc_; }

    /**
     * @brief Get the command at a specific index
     */
    const VMCommand* get_command(size_t index) const;

    /**
     * @brief Get the current command (at PC)
     */
    const VMCommand* get_current_command() const { return get_command(pc_); }

    /**
     * @brief Get total number of commands in the program
     */
    size_t get_command_count() const { return program_.commands.size(); }

    /**
     * @brief Get current function name
     */
    std::string get_current_function() const { return memory_.current_function(); }

    /**
     * @brief Get the call stack
     */
    const std::vector<CallFrame>& get_call_stack() const {
        return memory_.call_stack();
    }

    /**
     * @brief Get execution statistics
     */
    const VMStats& get_stats() const { return stats_; }

    // =========================================================================
    // Memory Access (delegated to VMMemory)
    // =========================================================================

    /**
     * @brief Get current stack contents
     */
    std::vector<Word> get_stack() const { return memory_.get_stack_contents(); }

    /**
     * @brief Get stack pointer
     */
    Word get_sp() const { return memory_.get_sp(); }

    /**
     * @brief Get a segment value
     */
    Word get_segment(SegmentType segment, uint16_t index,
                     const std::string& file_name = "") const {
        return memory_.read_segment(segment, index, file_name);
    }

    /**
     * @brief Read RAM directly
     */
    Word read_ram(Address address) const { return memory_.read_ram(address); }

    /**
     * @brief Write RAM directly (use with caution!)
     */
    void write_ram(Address address, Word value) { memory_.write_ram(address, value); }

    /**
     * @brief Get memory object for direct access
     */
    const VMMemory& memory() const { return memory_; }
    VMMemory& memory() { return memory_; }

    // =========================================================================
    // Breakpoints
    // =========================================================================

    /**
     * @brief Add a breakpoint at a command index
     *
     * @param command_index Index of the command to break at
     */
    void add_breakpoint(size_t command_index);

    /**
     * @brief Add a breakpoint in a specific function
     *
     * @param function_name Function to break in
     * @param offset Command offset within the function (0 = entry)
     */
    void add_function_breakpoint(const std::string& function_name, size_t offset = 0);

    /**
     * @brief Remove a breakpoint
     */
    void remove_breakpoint(size_t command_index);

    /**
     * @brief Clear all breakpoints
     */
    void clear_breakpoints();

    /**
     * @brief Check if there's a breakpoint at an index
     */
    bool has_breakpoint(size_t command_index) const;

    /**
     * @brief Get all breakpoint locations
     */
    std::vector<size_t> get_breakpoints() const;

    // =========================================================================
    // I/O (delegated to VMMemory)
    // =========================================================================

    /**
     * @brief Get screen buffer for rendering
     */
    const Word* get_screen_buffer() const { return memory_.screen_buffer(); }

    /**
     * @brief Get keyboard state
     */
    Word get_keyboard() const { return memory_.get_keyboard(); }

    /**
     * @brief Set keyboard state (simulate key press)
     */
    void set_keyboard(Word key_code) { memory_.set_keyboard(key_code); }

    // =========================================================================
    // Error Information
    // =========================================================================

    /**
     * @brief Get last error message (if state is ERROR)
     */
    const std::string& get_error_message() const { return error_message_; }

    /**
     * @brief Get the command index where error occurred
     */
    size_t get_error_location() const { return error_location_; }

private:
    // =========================================================================
    // Internal State
    // =========================================================================

    VMProgram program_;          // The loaded program
    VMMemory memory_;            // Memory system
    size_t pc_ = 0;              // Program counter
    VMState state_ = VMState::READY;
    PauseReason pause_reason_ = PauseReason::NONE;
    VMStats stats_;

    std::string entry_point_;    // Entry function name
    bool pause_requested_ = false;

    // Breakpoints
    std::unordered_set<size_t> breakpoints_;

    // Error state
    std::string error_message_;
    size_t error_location_ = 0;

    // =========================================================================
    // Execution Helpers
    // =========================================================================

    /**
     * @brief Execute a single command (internal)
     *
     * Updates PC and stats. Returns true if execution should continue.
     */
    bool execute_command();

    /**
     * @brief Execute an arithmetic command
     */
    void execute_arithmetic(const ArithmeticCommand& cmd);

    /**
     * @brief Execute a push command
     */
    void execute_push(const PushCommand& cmd);

    /**
     * @brief Execute a pop command
     */
    void execute_pop(const PopCommand& cmd);

    /**
     * @brief Execute a label command (no-op, just moves PC)
     */
    void execute_label(const LabelCommand& cmd);

    /**
     * @brief Execute a goto command
     */
    void execute_goto(const GotoCommand& cmd);

    /**
     * @brief Execute an if-goto command
     */
    void execute_if_goto(const IfGotoCommand& cmd);

    /**
     * @brief Execute a function definition
     */
    void execute_function(const FunctionCommand& cmd);

    /**
     * @brief Execute a function call
     */
    void execute_call(const CallCommand& cmd);

    /**
     * @brief Execute a return
     */
    void execute_return(const ReturnCommand& cmd);

    /**
     * @brief Look up a label and return its command index
     */
    size_t lookup_label(const std::string& label_name) const;

    /**
     * @brief Look up a function and return its entry point
     */
    size_t lookup_function(const std::string& function_name) const;

    /**
     * @brief Set error state with message
     */
    void set_error(const std::string& message);

    /**
     * @brief Initialize execution at entry point
     */
    void initialize_execution();
};

}  // namespace n2t

#endif  // NAND2TETRIS_VM_ENGINE_HPP
