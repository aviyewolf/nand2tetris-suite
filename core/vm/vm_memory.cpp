// ==============================================================================
// VM Memory Implementation
// ==============================================================================

#include "vm_memory.hpp"
#include <sstream>
#include <iomanip>

namespace n2t {

// ==============================================================================
// Constructor and Initialization
// ==============================================================================

VMMemory::VMMemory() {
    reset();
}

void VMMemory::reset() {
    // Clear all RAM
    ram_.fill(0);

    // Initialize stack pointer to stack base
    ram_[VMAddress::SP] = VMAddress::STACK_BASE;

    // Clear call stack
    call_stack_.clear();

    // Clear static allocations
    static_bases_.clear();
    next_static_address_ = VMAddress::STATIC_BASE;
}

void VMMemory::setup_call(const std::string& function_name, uint16_t num_args) {
    // Set up initial frame for the entry function
    // This is like the OS calling our main function
    CallFrame frame;
    frame.return_address = 0;  // Special: return address 0 means "halt"
    frame.function_name = function_name;
    frame.num_args = num_args;
    frame.num_locals = 0;  // Will be set when we see the function command
    frame.saved_lcl = 0;
    frame.saved_arg = 0;
    frame.saved_this = 0;
    frame.saved_that = 0;
    call_stack_.push_back(frame);
}

// ==============================================================================
// Stack Operations
// ==============================================================================

void VMMemory::push(Word value) {
    Word sp = ram_[VMAddress::SP];

    // Check for stack overflow
    if (sp > VMAddress::STACK_MAX) {
        throw RuntimeError(
            "Stack overflow! SP = " + std::to_string(sp) +
            ". The stack has exceeded its maximum size (2047). "
            "This usually means infinite recursion or too many nested function calls."
        );
    }

    // Push value and increment SP
    ram_[sp] = value;
    ram_[VMAddress::SP] = sp + 1;
}

Word VMMemory::pop() {
    Word sp = ram_[VMAddress::SP];

    // Check for stack underflow
    if (sp <= VMAddress::STACK_BASE) {
        throw RuntimeError(
            "Stack underflow! Attempted to pop from empty stack. "
            "This usually means there's a pop without a matching push, "
            "or a function returned without pushing a return value."
        );
    }

    // Decrement SP and return value
    ram_[VMAddress::SP] = sp - 1;
    return ram_[sp - 1];
}

Word VMMemory::peek() const {
    Word sp = ram_[VMAddress::SP];

    if (sp <= VMAddress::STACK_BASE) {
        throw RuntimeError("Cannot peek at empty stack");
    }

    return ram_[sp - 1];
}

// ==============================================================================
// Segment Access
// ==============================================================================

Word VMMemory::read_segment(SegmentType segment, uint16_t index,
                            const std::string& file_name) const {
    // Special case: constant segment just returns the index value
    if (segment == SegmentType::CONSTANT) {
        return index;
    }

    Address addr = calculate_address(segment, index, file_name);
    return ram_[addr];
}

void VMMemory::write_segment(SegmentType segment, uint16_t index, Word value,
                             const std::string& file_name) {
    // Cannot write to constant segment
    if (segment == SegmentType::CONSTANT) {
        throw RuntimeError(
            "Cannot write to constant segment. "
            "Constants are read-only values, not memory locations."
        );
    }

    Address addr = calculate_address(segment, index, file_name);
    ram_[addr] = value;
}

Word VMMemory::get_segment_base(SegmentType segment) const {
    switch (segment) {
        case SegmentType::LOCAL:
            return ram_[VMAddress::LCL];
        case SegmentType::ARGUMENT:
            return ram_[VMAddress::ARG];
        case SegmentType::THIS:
            return ram_[VMAddress::THIS];
        case SegmentType::THAT:
            return ram_[VMAddress::THAT];
        case SegmentType::TEMP:
            return VMAddress::TEMP_BASE;
        case SegmentType::STATIC:
            return VMAddress::STATIC_BASE;
        case SegmentType::POINTER:
            return VMAddress::THIS;  // pointer segment accesses THIS/THAT
        case SegmentType::CONSTANT:
            return 0;  // constant has no base
        default:
            return 0;
    }
}

Address VMMemory::calculate_address(SegmentType segment, uint16_t index,
                                    const std::string& file_name) const {
    switch (segment) {
        case SegmentType::LOCAL: {
            Word base = ram_[VMAddress::LCL];
            return base + index;
        }

        case SegmentType::ARGUMENT: {
            Word base = ram_[VMAddress::ARG];
            return base + index;
        }

        case SegmentType::THIS: {
            Word base = ram_[VMAddress::THIS];
            return base + index;
        }

        case SegmentType::THAT: {
            Word base = ram_[VMAddress::THAT];
            return base + index;
        }

        case SegmentType::TEMP: {
            if (index >= VMAddress::TEMP_SIZE) {
                throw RuntimeError(
                    "Temp segment index out of bounds: " + std::to_string(index) +
                    ". Valid range is 0-7."
                );
            }
            return VMAddress::TEMP_BASE + index;
        }

        case SegmentType::STATIC: {
            // Each file has its own static segment
            // We need to look up or allocate the base for this file
            Address base;
            auto it = static_bases_.find(file_name);
            if (it != static_bases_.end()) {
                base = it->second;
            } else {
                // This is a const method, so we can't allocate here
                // In practice, the execution engine will allocate first
                throw RuntimeError(
                    "Static segment for file '" + file_name + "' not initialized"
                );
            }
            return base + index;
        }

        case SegmentType::POINTER: {
            if (index > 1) {
                throw RuntimeError(
                    "Pointer segment index out of bounds: " + std::to_string(index) +
                    ". Valid range is 0-1 (0=THIS, 1=THAT)."
                );
            }
            // pointer 0 = THIS, pointer 1 = THAT
            return VMAddress::THIS + index;
        }

        case SegmentType::CONSTANT:
            // This should be handled by the caller
            throw InternalError("calculate_address called for constant segment");

        default:
            throw InternalError("Unknown segment type");
    }
}

Address VMMemory::get_static_base(const std::string& file_name) {
    // Check if already allocated
    auto it = static_bases_.find(file_name);
    if (it != static_bases_.end()) {
        return it->second;
    }

    // Allocate new static segment
    Address base = next_static_address_;

    // Check if we've run out of static space
    if (base >= VMAddress::STACK_BASE) {
        throw RuntimeError(
            "Out of static variable space! Too many static variables across all files."
        );
    }

    static_bases_[file_name] = base;

    // Reserve some space for this file's statics (we don't know how many it needs)
    // We'll allocate 16 per file by default
    next_static_address_ += 16;

    return base;
}

// ==============================================================================
// Direct RAM Access
// ==============================================================================

Word VMMemory::read_ram(Address address) const {
    if (address >= VMAddress::RAM_SIZE) {
        throw RuntimeError(
            "Memory access out of bounds: " + std::to_string(address) +
            ". Valid range is 0-32767."
        );
    }
    return ram_[address];
}

void VMMemory::write_ram(Address address, Word value) {
    if (address >= VMAddress::RAM_SIZE) {
        throw RuntimeError(
            "Memory write out of bounds: " + std::to_string(address) +
            ". Valid range is 0-32767."
        );
    }
    ram_[address] = value;
}

// ==============================================================================
// Function Call Support
// ==============================================================================

void VMMemory::push_frame(size_t return_address, const std::string& function_name,
                          uint16_t num_args, uint16_t num_locals) {
    // Save current frame state
    CallFrame frame;
    frame.return_address = return_address;
    frame.function_name = function_name;
    frame.num_args = num_args;
    frame.num_locals = num_locals;

    // Save current segment pointers
    frame.saved_lcl = ram_[VMAddress::LCL];
    frame.saved_arg = ram_[VMAddress::ARG];
    frame.saved_this = ram_[VMAddress::THIS];
    frame.saved_that = ram_[VMAddress::THAT];

    // Push saved state onto stack (following VM spec)
    // The caller has already pushed the arguments
    Word sp = ram_[VMAddress::SP];

    // Push return address
    ram_[sp++] = static_cast<Word>(return_address);

    // Push saved LCL, ARG, THIS, THAT
    ram_[sp++] = frame.saved_lcl;
    ram_[sp++] = frame.saved_arg;
    ram_[sp++] = frame.saved_this;
    ram_[sp++] = frame.saved_that;

    // Set ARG to point to first argument
    // ARG = SP - num_args - 5 (5 = return addr + 4 saved pointers)
    ram_[VMAddress::ARG] = static_cast<Word>(sp - num_args - 5);

    // Set LCL to current SP (start of local segment)
    ram_[VMAddress::LCL] = sp;

    // Initialize local variables to 0 and update SP
    for (uint16_t i = 0; i < num_locals; i++) {
        ram_[sp++] = 0;
    }
    ram_[VMAddress::SP] = sp;

    // Add frame to call stack
    call_stack_.push_back(frame);
}

size_t VMMemory::pop_frame(Word return_value) {
    if (call_stack_.empty()) {
        throw RuntimeError(
            "Attempted to return but no function is active. "
            "This usually means a 'return' without a matching 'call'."
        );
    }

    // Get the frame we're returning from
    CallFrame frame = call_stack_.back();
    call_stack_.pop_back();

    // Get pointer to start of frame (saved LCL value location)
    Word frame_ptr = ram_[VMAddress::LCL];

    // IMPORTANT: Save return address BEFORE writing return value,
    // because ARG[0] and frame_ptr-5 may overlap when num_args == 0.
    size_t ret_addr = ram_[frame_ptr - 5];

    // Save ARG before restoring pointers
    Word arg_addr = ram_[VMAddress::ARG];

    // Restore segment pointers from saved values on stack
    ram_[VMAddress::THAT] = ram_[frame_ptr - 1];
    ram_[VMAddress::THIS] = ram_[frame_ptr - 2];
    ram_[VMAddress::ARG]  = ram_[frame_ptr - 3];
    ram_[VMAddress::LCL]  = ram_[frame_ptr - 4];

    // Place return value where caller expects it (at ARG[0])
    ram_[arg_addr] = return_value;

    // Restore SP to just after return value
    ram_[VMAddress::SP] = arg_addr + 1;

    return ret_addr;
}

const CallFrame* VMMemory::current_frame() const {
    if (call_stack_.empty()) {
        return nullptr;
    }
    return &call_stack_.back();
}

std::string VMMemory::current_function() const {
    if (call_stack_.empty()) {
        return "";
    }
    return call_stack_.back().function_name;
}

// ==============================================================================
// I/O Access
// ==============================================================================

bool VMMemory::get_pixel(int x, int y) const {
    if (x < 0 || x >= 512 || y < 0 || y >= 256) {
        return false;  // Out of bounds pixels are off
    }

    // Calculate word address: each row is 32 words, each word is 16 pixels
    int word_offset = (y * 32) + (x / 16);
    int bit_offset = x % 16;

    Word word = ram_[VMAddress::SCREEN_BASE + word_offset];
    return (word >> bit_offset) & 1;
}

void VMMemory::set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= 512 || y < 0 || y >= 256) {
        return;  // Ignore out of bounds
    }

    int word_offset = (y * 32) + (x / 16);
    int bit_offset = x % 16;

    Address addr = static_cast<Address>(VMAddress::SCREEN_BASE + word_offset);
    if (on) {
        ram_[addr] |= (1 << bit_offset);   // Set bit
    } else {
        ram_[addr] &= static_cast<Word>(~(1 << bit_offset));  // Clear bit
    }
}

// ==============================================================================
// Debugging Support
// ==============================================================================

std::vector<Word> VMMemory::get_stack_contents() const {
    std::vector<Word> contents;
    Word sp = ram_[VMAddress::SP];

    for (Address addr = VMAddress::STACK_BASE; addr < sp; addr++) {
        contents.push_back(ram_[addr]);
    }

    return contents;
}

std::vector<Word> VMMemory::get_segment_contents(SegmentType segment, size_t count) const {
    std::vector<Word> contents;
    Word base = get_segment_base(segment);

    for (size_t i = 0; i < count; i++) {
        if (base + i >= VMAddress::RAM_SIZE) break;
        contents.push_back(ram_[base + i]);
    }

    return contents;
}

std::string VMMemory::dump_state() const {
    std::ostringstream oss;

    // Special registers
    oss << "=== VM Memory State ===\n";
    oss << "SP   = " << std::setw(5) << ram_[VMAddress::SP] << "\n";
    oss << "LCL  = " << std::setw(5) << ram_[VMAddress::LCL] << "\n";
    oss << "ARG  = " << std::setw(5) << ram_[VMAddress::ARG] << "\n";
    oss << "THIS = " << std::setw(5) << ram_[VMAddress::THIS] << "\n";
    oss << "THAT = " << std::setw(5) << ram_[VMAddress::THAT] << "\n";

    // Stack contents
    oss << "\n=== Stack ===\n";
    auto stack = get_stack_contents();
    if (stack.empty()) {
        oss << "(empty)\n";
    } else {
        for (size_t i = 0; i < stack.size(); i++) {
            oss << "[" << std::setw(3) << i << "] " << stack[i] << "\n";
        }
    }

    // Call stack
    oss << "\n=== Call Stack ===\n";
    if (call_stack_.empty()) {
        oss << "(empty)\n";
    } else {
        for (size_t i = 0; i < call_stack_.size(); i++) {
            const auto& frame = call_stack_[i];
            oss << "[" << i << "] " << frame.function_name
                << " (args=" << frame.num_args
                << ", locals=" << frame.num_locals << ")\n";
        }
    }

    return oss.str();
}

}  // namespace n2t
