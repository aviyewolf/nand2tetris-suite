// ==============================================================================
// CPU Memory System
// ==============================================================================
// Manages the Hack computer's memory subsystems:
//   - ROM (32K): Read-only instruction memory, loaded from .hack files
//   - RAM (32K): Read/write data memory
//   - Screen (memory-mapped): 256x512 pixels at addresses 16384-24575
//   - Keyboard (memory-mapped): Currently pressed key at address 24576
//
// This is a hardware-level memory model — no segment abstractions.
// The VM-level segments (local, argument, etc.) are just conventions
// about how RAM addresses are used.
// ==============================================================================

#ifndef NAND2TETRIS_CPU_MEMORY_HPP
#define NAND2TETRIS_CPU_MEMORY_HPP

#include "types.hpp"
#include "error.hpp"
#include <array>
#include <vector>
#include <string>

namespace n2t {

// ==============================================================================
// CPU Memory Address Constants
// ==============================================================================

namespace CPUAddress {
    // Special RAM addresses (same layout as VMAddress)
    constexpr Address SP   = 0;
    constexpr Address LCL  = 1;
    constexpr Address ARG  = 2;
    constexpr Address THIS = 3;
    constexpr Address THAT = 4;
    constexpr Address TEMP_BASE = 5;
    constexpr Address R13  = 13;
    constexpr Address R14  = 14;
    constexpr Address R15  = 15;
    constexpr Address STATIC_BASE = 16;
    constexpr Address STACK_BASE = 256;

    // Screen memory-mapped I/O
    constexpr Address SCREEN_BASE = 16384;
    constexpr size_t  SCREEN_SIZE = 8192;  // 256 rows * 32 words/row

    // Keyboard memory-mapped I/O
    constexpr Address KEYBOARD = 24576;

    // Memory sizes
    constexpr size_t RAM_SIZE = 32768;  // 32K words
    constexpr size_t ROM_SIZE = 32768;  // 32K instructions
}

// ==============================================================================
// CPU Memory Class
// ==============================================================================

/**
 * @brief Hardware-level memory for the Hack computer.
 *
 * Provides separate ROM (instruction memory) and RAM (data memory)
 * following the Harvard architecture of the Hack platform.
 */
class CPUMemory {
public:
    CPUMemory();

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Reset all memory to zeros.
     */
    void reset();

    // =========================================================================
    // ROM (Instruction Memory)
    // =========================================================================

    /**
     * @brief Load a program from a .hack file.
     *
     * A .hack file contains one instruction per line, each as a
     * 16-character string of '0' and '1' characters.
     *
     * @param file_path Path to the .hack file
     * @throws FileError if file cannot be read
     * @throws ParseError if file contains invalid instructions
     */
    void load_rom_file(const std::string& file_path);

    /**
     * @brief Load a program from a string of binary lines.
     *
     * @param hack_text Text with one 16-bit binary string per line
     * @throws ParseError if text contains invalid instructions
     */
    void load_rom_string(const std::string& hack_text);

    /**
     * @brief Load a program from pre-parsed instruction words.
     */
    void load_rom(const std::vector<Word>& instructions);

    /**
     * @brief Read an instruction from ROM.
     */
    Word read_rom(Address address) const;

    /**
     * @brief Get the number of instructions loaded.
     */
    size_t rom_size() const { return program_size_; }

    /**
     * @brief Get raw ROM pointer for disassembly views.
     */
    const Word* rom_ptr() const { return rom_.data(); }

    // =========================================================================
    // RAM (Data Memory)
    // =========================================================================

    /**
     * @brief Read a value from RAM.
     *
     * @param address Memory address (0-32767)
     * @return Value at that address
     * @throws RuntimeError if address is out of bounds
     */
    Word read_ram(Address address) const;

    /**
     * @brief Write a value to RAM.
     *
     * @param address Memory address (0-32767)
     * @param value Value to write
     * @throws RuntimeError if address is out of bounds
     */
    void write_ram(Address address, Word value);

    /**
     * @brief Get raw RAM pointer for bulk access.
     */
    const Word* ram_ptr() const { return ram_.data(); }

    // =========================================================================
    // I/O (Screen + Keyboard)
    // =========================================================================

    /**
     * @brief Get pointer to screen memory (8192 words).
     */
    const Word* screen_buffer() const {
        return &ram_[CPUAddress::SCREEN_BASE];
    }

    /**
     * @brief Check if a pixel is on (black).
     */
    bool get_pixel(int x, int y) const;

    /**
     * @brief Set a pixel on or off.
     */
    void set_pixel(int x, int y, bool on);

    /**
     * @brief Get currently pressed key.
     */
    Word get_keyboard() const { return ram_[CPUAddress::KEYBOARD]; }

    /**
     * @brief Simulate a key press.
     */
    void set_keyboard(Word key_code) { ram_[CPUAddress::KEYBOARD] = key_code; }

    /**
     * @brief Check if screen memory was modified since last check.
     */
    bool screen_dirty() const { return screen_dirty_; }

    /**
     * @brief Clear the screen dirty flag.
     */
    void clear_screen_dirty() { screen_dirty_ = false; }

    // =========================================================================
    // Debugging
    // =========================================================================

    /**
     * @brief Dump memory state for debugging.
     */
    std::string dump_state() const;

private:
    std::array<Word, CPUAddress::ROM_SIZE> rom_;
    std::array<Word, CPUAddress::RAM_SIZE> ram_;
    size_t program_size_ = 0;
    bool screen_dirty_ = false;

    /**
     * @brief Parse a single line of binary text into a Word.
     */
    Word parse_binary_line(const std::string& line, size_t line_number);
};

}  // namespace n2t

#endif  // NAND2TETRIS_CPU_MEMORY_HPP
