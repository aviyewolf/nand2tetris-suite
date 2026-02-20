// ==============================================================================
// CPU Memory Implementation
// ==============================================================================

#include "memory.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace n2t {

// ==============================================================================
// Constructor and Initialization
// ==============================================================================

CPUMemory::CPUMemory() {
    reset();
}

void CPUMemory::reset() {
    rom_.fill(0);
    ram_.fill(0);
    program_size_ = 0;
    screen_dirty_ = false;
}

// ==============================================================================
// ROM Loading
// ==============================================================================

void CPUMemory::load_rom_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw FileError(file_path, "Could not open .hack file for reading");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    load_rom_string(buffer.str());
}

void CPUMemory::load_rom_string(const std::string& hack_text) {
    rom_.fill(0);
    program_size_ = 0;

    std::istringstream stream(hack_text);
    std::string line;
    size_t line_number = 0;

    while (std::getline(stream, line)) {
        line_number++;

        // Trim trailing whitespace/CR
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty()) continue;

        if (program_size_ >= CPUAddress::ROM_SIZE) {
            throw ParseError("<rom>", line_number,
                "Program too large! ROM can hold at most " +
                std::to_string(CPUAddress::ROM_SIZE) + " instructions.");
        }

        rom_[program_size_] = parse_binary_line(line, line_number);
        program_size_++;
    }
}

void CPUMemory::load_rom(const std::vector<Word>& instructions) {
    rom_.fill(0);

    if (instructions.size() > CPUAddress::ROM_SIZE) {
        throw RuntimeError(
            "Program too large! ROM can hold at most " +
            std::to_string(CPUAddress::ROM_SIZE) + " instructions, got " +
            std::to_string(instructions.size()) + ".");
    }

    for (size_t i = 0; i < instructions.size(); i++) {
        rom_[i] = instructions[i];
    }
    program_size_ = instructions.size();
}

Word CPUMemory::parse_binary_line(const std::string& line, size_t line_number) {
    if (line.length() != 16) {
        throw ParseError("<rom>", line_number,
            "Expected 16-bit binary instruction (16 characters of '0' and '1'), "
            "got " + std::to_string(line.length()) + " characters: \"" + line + "\"");
    }

    Word result = 0;
    for (size_t i = 0; i < 16; i++) {
        char c = line[i];
        if (c == '1') {
            result |= static_cast<Word>(1 << (15 - i));
        } else if (c != '0') {
            throw ParseError("<rom>", line_number,
                "Invalid character '" + std::string(1, c) +
                "' at position " + std::to_string(i + 1) +
                ". Only '0' and '1' are allowed in .hack files.");
        }
    }

    return result;
}

// ==============================================================================
// ROM Access
// ==============================================================================

Word CPUMemory::read_rom(Address address) const {
    if (address >= CPUAddress::ROM_SIZE) {
        throw RuntimeError(
            "ROM access out of bounds: address " + std::to_string(address) +
            ". Valid range is 0-" + std::to_string(CPUAddress::ROM_SIZE - 1) + ".");
    }
    return rom_[address];
}

// ==============================================================================
// RAM Access
// ==============================================================================

Word CPUMemory::read_ram(Address address) const {
    if (address >= CPUAddress::RAM_SIZE) {
        throw RuntimeError(
            "Cannot read RAM at address " + std::to_string(address) +
            ". Valid range is 0-32767 (32K). "
            "The A register may contain an out-of-bounds value.");
    }
    return ram_[address];
}

void CPUMemory::write_ram(Address address, Word value) {
    if (address >= CPUAddress::RAM_SIZE) {
        throw RuntimeError(
            "Cannot write to RAM at address " + std::to_string(address) +
            ". Valid range is 0-32767 (32K). "
            "The A register may contain an out-of-bounds value.");
    }

    ram_[address] = value;

    // Track screen modifications
    if (address >= CPUAddress::SCREEN_BASE &&
        address < CPUAddress::SCREEN_BASE + CPUAddress::SCREEN_SIZE) {
        screen_dirty_ = true;
    }
}

// ==============================================================================
// I/O
// ==============================================================================

bool CPUMemory::get_pixel(int x, int y) const {
    if (x < 0 || x >= 512 || y < 0 || y >= 256) {
        return false;
    }

    int word_offset = (y * 32) + (x / 16);
    int bit_offset = x % 16;

    Word word = ram_[CPUAddress::SCREEN_BASE + word_offset];
    return (word >> bit_offset) & 1;
}

void CPUMemory::set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= 512 || y < 0 || y >= 256) {
        return;
    }

    int word_offset = (y * 32) + (x / 16);
    int bit_offset = x % 16;

    Address addr = static_cast<Address>(CPUAddress::SCREEN_BASE + word_offset);
    if (on) {
        ram_[addr] |= static_cast<Word>(1 << bit_offset);
    } else {
        ram_[addr] &= static_cast<Word>(~(1 << bit_offset));
    }
    screen_dirty_ = true;
}

// ==============================================================================
// Debugging
// ==============================================================================

std::string CPUMemory::dump_state() const {
    std::ostringstream oss;

    oss << "=== CPU Memory State ===\n";
    oss << "ROM: " << program_size_ << " instructions loaded\n";

    // Special registers (R0-R15)
    oss << "\n--- Registers (RAM 0-15) ---\n";
    const char* reg_names[] = {
        "SP", "LCL", "ARG", "THIS", "THAT",
        "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
        "R13", "R14", "R15"
    };
    for (int i = 0; i < 16; i++) {
        oss << std::setw(4) << reg_names[i] << " = "
            << std::setw(6) << ram_[static_cast<size_t>(i)] << "\n";
    }

    // Stack peek (first few values)
    Word sp = ram_[CPUAddress::SP];
    if (sp > CPUAddress::STACK_BASE) {
        oss << "\n--- Stack (top 5) ---\n";
        size_t count = 0;
        for (Address addr = sp - 1; addr >= CPUAddress::STACK_BASE && count < 5; addr--, count++) {
            oss << "[" << addr << "] = " << ram_[addr] << "\n";
            if (addr == 0) break;  // Prevent underflow
        }
    }

    oss << "\nScreen dirty: " << (screen_dirty_ ? "yes" : "no") << "\n";
    oss << "Keyboard: " << ram_[CPUAddress::KEYBOARD] << "\n";

    return oss.str();
}

}  // namespace n2t
