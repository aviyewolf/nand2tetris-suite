// ==============================================================================
// VM Memory System
// ==============================================================================
// The VM memory manages:
// - The global stack (where all computations happen)
// - Memory segments (local, argument, this, that, static, temp, pointer)
// - The call frame stack (for function calls)
//
// Memory Layout (Hack RAM):
//   0-15:       Special addresses (SP, LCL, ARG, THIS, THAT, R5-R15)
//   16-255:     Static variables
//   256-2047:   Stack space
//   2048-16383: Heap (objects and arrays)
//   16384-24575: Screen memory-mapped I/O
//   24576:      Keyboard memory-mapped I/O
// ==============================================================================

#ifndef NAND2TETRIS_VM_MEMORY_HPP
#define NAND2TETRIS_VM_MEMORY_HPP

#include "types.hpp"
#include "error.hpp"
#include <array>
#include <vector>
#include <string>
#include <unordered_map>

namespace n2t {

// ==============================================================================
// Memory Constants
// ==============================================================================

/**
 * Memory addresses for special pointers
 *
 * These are fixed addresses in Hack RAM where the VM stores
 * pointers to the various memory segments.
 */
namespace VMAddress {
    constexpr Address SP   = 0;    // Stack Pointer
    constexpr Address LCL  = 1;    // Local segment base
    constexpr Address ARG  = 2;    // Argument segment base
    constexpr Address THIS = 3;    // This segment base (current object)
    constexpr Address THAT = 4;    // That segment base (current array)

    // Temp segment is fixed at addresses 5-12
    constexpr Address TEMP_BASE = 5;
    constexpr Address TEMP_SIZE = 8;

    // General purpose registers (used internally by VM translator)
    constexpr Address R13 = 13;
    constexpr Address R14 = 14;
    constexpr Address R15 = 15;

    // Static segment starts at address 16
    constexpr Address STATIC_BASE = 16;
    constexpr Address STATIC_SIZE = 240;  // 16-255

    // Stack starts at address 256
    constexpr Address STACK_BASE = 256;
    constexpr Address STACK_MAX = 2047;

    // Heap starts at address 2048
    constexpr Address HEAP_BASE = 2048;
    constexpr Address HEAP_MAX = 16383;

    // Screen memory-mapped I/O
    constexpr Address SCREEN_BASE = 16384;
    constexpr Address SCREEN_SIZE = 8192;  // 256 rows * 32 words/row

    // Keyboard memory-mapped I/O
    constexpr Address KEYBOARD = 24576;

    // Total RAM size
    constexpr size_t RAM_SIZE = 32768;
}

// ==============================================================================
// Call Frame - Saved State for Function Calls
// ==============================================================================

/**
 * @brief Represents a function call frame
 *
 * When a function is called, we save the caller's state so we can
 * restore it when the function returns.
 *
 * The frame contains:
 * - Return address (where to continue after return)
 * - Saved segment pointers (LCL, ARG, THIS, THAT)
 * - Function name and argument count (for debugging)
 */
struct CallFrame {
    // Where to return after function completes
    size_t return_address;

    // Saved segment base pointers
    Word saved_lcl;
    Word saved_arg;
    Word saved_this;
    Word saved_that;

    // For debugging: which function this frame belongs to
    std::string function_name;
    uint16_t num_args;
    uint16_t num_locals;

    // For source-level debugging: where the call originated
    std::string caller_file;
    LineNumber caller_line;
};

// ==============================================================================
// VM Memory Class
// ==============================================================================

/**
 * @brief Manages VM memory (RAM, stack, segments)
 *
 * This class provides a high-level interface for VM memory operations:
 * - Push/pop values on the stack
 * - Read/write memory segments
 * - Manage function call frames
 * - Access screen and keyboard I/O
 *
 * The memory system enforces bounds checking and provides helpful
 * error messages when students' code accesses invalid memory.
 */
class VMMemory {
public:
    VMMemory();

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Reset memory to initial state
     *
     * - Clears all RAM
     * - Sets SP to 256 (stack base)
     * - Clears call frame stack
     */
    void reset();

    /**
     * @brief Set up for running a specific function
     *
     * Used to bootstrap execution by setting up the initial call frame.
     *
     * @param function_name Name of the entry point function
     * @param num_args Number of arguments (usually 0 for main)
     */
    void setup_call(const std::string& function_name, uint16_t num_args);

    // =========================================================================
    // Stack Operations
    // =========================================================================

    /**
     * @brief Push a value onto the stack
     *
     * Increments SP and stores value at the new top of stack.
     *
     * @param value The value to push
     * @throws RuntimeError if stack overflow
     */
    void push(Word value);

    /**
     * @brief Pop a value from the stack
     *
     * Returns the top value and decrements SP.
     *
     * @return The popped value
     * @throws RuntimeError if stack underflow
     */
    Word pop();

    /**
     * @brief Peek at the top of the stack without popping
     *
     * @return The value at the top of the stack
     * @throws RuntimeError if stack is empty
     */
    Word peek() const;

    /**
     * @brief Get current stack pointer
     */
    Word get_sp() const { return ram_[VMAddress::SP]; }

    /**
     * @brief Get the number of values currently on the stack
     */
    size_t stack_size() const { return get_sp() - VMAddress::STACK_BASE; }

    /**
     * @brief Check if stack is empty
     */
    bool stack_empty() const { return get_sp() <= VMAddress::STACK_BASE; }

    // =========================================================================
    // Segment Access
    // =========================================================================

    /**
     * @brief Read a value from a memory segment
     *
     * @param segment Which segment to read from
     * @param index Index within the segment
     * @param file_name For static segment: which file's statics
     * @return The value at segment[index]
     * @throws RuntimeError if index is out of bounds
     */
    Word read_segment(SegmentType segment, uint16_t index,
                      const std::string& file_name = "") const;

    /**
     * @brief Write a value to a memory segment
     *
     * @param segment Which segment to write to
     * @param index Index within the segment
     * @param value The value to write
     * @param file_name For static segment: which file's statics
     * @throws RuntimeError if segment is constant or index is out of bounds
     */
    void write_segment(SegmentType segment, uint16_t index, Word value,
                       const std::string& file_name = "");

    /**
     * @brief Get the base address of a segment
     *
     * Useful for debugging and visualization.
     */
    Word get_segment_base(SegmentType segment) const;

    // =========================================================================
    // Direct RAM Access
    // =========================================================================

    /**
     * @brief Read directly from RAM
     *
     * @param address Memory address (0-32767)
     * @return Value at that address
     */
    Word read_ram(Address address) const;

    /**
     * @brief Write directly to RAM
     *
     * @param address Memory address (0-32767)
     * @param value Value to write
     */
    void write_ram(Address address, Word value);

    /**
     * @brief Get pointer to RAM for bulk access
     *
     * Useful for screen rendering and memory dumps.
     */
    const Word* ram_ptr() const { return ram_.data(); }

    // =========================================================================
    // Function Call Support
    // =========================================================================

    /**
     * @brief Push a new call frame (called by CALL instruction)
     *
     * Saves the current state and sets up for the called function.
     *
     * @param return_address Where to return after function completes
     * @param function_name Name of function being called
     * @param num_args Number of arguments passed
     * @param num_locals Number of local variables in the function
     */
    void push_frame(size_t return_address, const std::string& function_name,
                    uint16_t num_args, uint16_t num_locals);

    /**
     * @brief Pop the current call frame (called by RETURN instruction)
     *
     * Restores the caller's state and puts return value in the right place.
     *
     * @param return_value The value to return to the caller
     * @return The return address (where to continue execution)
     * @throws RuntimeError if no frame to pop (return from nowhere)
     */
    size_t pop_frame(Word return_value);

    /**
     * @brief Get the current call frame (for debugging)
     *
     * @return Pointer to current frame, or nullptr if no active call
     */
    const CallFrame* current_frame() const;

    /**
     * @brief Get the entire call stack (for debugging)
     *
     * The first element is the oldest frame (main), the last is the current.
     */
    const std::vector<CallFrame>& call_stack() const { return call_stack_; }

    /**
     * @brief Get current function name
     */
    std::string current_function() const;

    // =========================================================================
    // I/O Access
    // =========================================================================

    /**
     * @brief Get screen memory as a buffer
     *
     * The screen is 256 rows x 512 pixels.
     * Each row is 32 words (512/16 = 32).
     * Each word holds 16 pixels (1 = black, 0 = white).
     *
     * @return Pointer to screen memory (8192 words)
     */
    const Word* screen_buffer() const {
        return &ram_[VMAddress::SCREEN_BASE];
    }

    /**
     * @brief Check if a pixel is on (black)
     *
     * @param x X coordinate (0-511)
     * @param y Y coordinate (0-255)
     * @return true if pixel is black
     */
    bool get_pixel(int x, int y) const;

    /**
     * @brief Set a pixel
     *
     * @param x X coordinate (0-511)
     * @param y Y coordinate (0-255)
     * @param on true for black, false for white
     */
    void set_pixel(int x, int y, bool on);

    /**
     * @brief Get currently pressed key (from keyboard buffer)
     *
     * @return ASCII code of pressed key, or 0 if no key pressed
     */
    Word get_keyboard() const { return ram_[VMAddress::KEYBOARD]; }

    /**
     * @brief Simulate a key press
     *
     * @param key_code ASCII code of the key (0 = no key)
     */
    void set_keyboard(Word key_code) { ram_[VMAddress::KEYBOARD] = key_code; }

    // =========================================================================
    // Debugging Support
    // =========================================================================

    /**
     * @brief Get the values currently on the stack
     *
     * Returns the stack contents from bottom to top.
     */
    std::vector<Word> get_stack_contents() const;

    /**
     * @brief Get values in a segment
     *
     * @param segment Which segment
     * @param count How many values to retrieve (starting from index 0)
     * @return Vector of values
     */
    std::vector<Word> get_segment_contents(SegmentType segment, size_t count) const;

    /**
     * @brief Dump memory state for debugging
     *
     * Returns a human-readable string showing memory state.
     */
    std::string dump_state() const;

private:
    // =========================================================================
    // Internal State
    // =========================================================================

    // Main RAM (32K words)
    std::array<Word, VMAddress::RAM_SIZE> ram_;

    // Call frame stack
    std::vector<CallFrame> call_stack_;

    // Static variable mapping: file_name -> base address
    // Each file gets its own static segment starting from STATIC_BASE
    std::unordered_map<std::string, Address> static_bases_;
    Address next_static_address_ = VMAddress::STATIC_BASE;

    // =========================================================================
    // Internal Helpers
    // =========================================================================

public:
    /**
     * @brief Get or allocate the static base for a file
     *
     * Called by VMEngine during initialization to pre-allocate
     * static segments for all loaded files.
     */
    Address get_static_base(const std::string& file_name);

private:

    /**
     * @brief Calculate the actual RAM address for a segment access
     *
     * @param segment The segment type
     * @param index Index within the segment
     * @param file_name For static segment
     * @return The RAM address
     */
    Address calculate_address(SegmentType segment, uint16_t index,
                              const std::string& file_name) const;
};

}  // namespace n2t

#endif  // NAND2TETRIS_VM_MEMORY_HPP
