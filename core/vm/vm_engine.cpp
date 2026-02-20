// ==============================================================================
// VM Execution Engine Implementation
// ==============================================================================
// Executes VM programs by dispatching commands to the appropriate handlers.
// Provides debugging facilities: stepping, breakpoints, state inspection.
// ==============================================================================

#include "vm_engine.hpp"
#include <algorithm>

namespace n2t {

// ==============================================================================
// Constructor
// ==============================================================================

VMEngine::VMEngine() = default;

// ==============================================================================
// Program Loading
// ==============================================================================

void VMEngine::load_file(const std::string& file_path) {
    VMParser parser;
    parser.parse_file(file_path);
    program_ = parser.get_program();
    state_ = VMState::READY;
    pc_ = 0;
    stats_.reset();
}

void VMEngine::load_string(const std::string& source,
                           const std::string& file_name) {
    VMParser parser;
    parser.parse_string(source, file_name);
    program_ = parser.get_program();
    state_ = VMState::READY;
    pc_ = 0;
    stats_.reset();
}

void VMEngine::load_directory(const std::string& directory_path) {
    VMParser parser;
    parser.parse_directory(directory_path);
    program_ = parser.get_program();
    state_ = VMState::READY;
    pc_ = 0;
    stats_.reset();
}

void VMEngine::set_entry_point(const std::string& function_name) {
    entry_point_ = function_name;
}

void VMEngine::reset() {
    memory_.reset();
    pc_ = 0;
    state_ = VMState::READY;
    pause_reason_ = PauseReason::NONE;
    pause_requested_ = false;
    stats_.reset();
    error_message_.clear();
    error_location_ = 0;
}

// ==============================================================================
// Execution Control
// ==============================================================================

VMState VMEngine::run() {
    if (state_ == VMState::READY) {
        initialize_execution();
    }

    if (state_ != VMState::PAUSED && state_ != VMState::RUNNING) {
        return state_;
    }

    state_ = VMState::RUNNING;
    pause_reason_ = PauseReason::NONE;
    pause_requested_ = false;

    while (state_ == VMState::RUNNING) {
        if (!execute_command()) {
            break;
        }
    }

    return state_;
}

VMState VMEngine::run_for(uint64_t max_instructions) {
    if (state_ == VMState::READY) {
        initialize_execution();
    }

    if (state_ != VMState::PAUSED && state_ != VMState::RUNNING) {
        return state_;
    }

    state_ = VMState::RUNNING;
    pause_reason_ = PauseReason::NONE;
    pause_requested_ = false;

    uint64_t count = 0;
    while (state_ == VMState::RUNNING && count < max_instructions) {
        if (!execute_command()) {
            break;
        }
        count++;
    }

    // If we hit the limit without halting or error, pause
    if (state_ == VMState::RUNNING) {
        state_ = VMState::PAUSED;
        pause_reason_ = PauseReason::USER_REQUEST;
    }

    return state_;
}

VMState VMEngine::step() {
    if (state_ == VMState::READY) {
        initialize_execution();
    }

    if (state_ != VMState::PAUSED && state_ != VMState::RUNNING) {
        return state_;
    }

    state_ = VMState::RUNNING;
    pause_reason_ = PauseReason::NONE;

    execute_command();

    // After single step, pause (unless we halted or errored)
    if (state_ == VMState::RUNNING) {
        state_ = VMState::PAUSED;
        pause_reason_ = PauseReason::STEP_COMPLETE;
    }

    return state_;
}

VMState VMEngine::step_out() {
    if (state_ == VMState::READY) {
        initialize_execution();
    }

    if (state_ != VMState::PAUSED && state_ != VMState::RUNNING) {
        return state_;
    }

    // Remember current call depth
    size_t target_depth = memory_.call_stack().size() - 1;

    state_ = VMState::RUNNING;
    pause_reason_ = PauseReason::NONE;
    pause_requested_ = false;

    while (state_ == VMState::RUNNING) {
        if (!execute_command()) {
            break;
        }
        // Stop when we've returned to a shallower call depth
        if (memory_.call_stack().size() <= target_depth) {
            state_ = VMState::PAUSED;
            pause_reason_ = PauseReason::FUNCTION_EXIT;
            break;
        }
    }

    return state_;
}

VMState VMEngine::step_over() {
    if (state_ == VMState::READY) {
        initialize_execution();
    }

    if (state_ != VMState::PAUSED && state_ != VMState::RUNNING) {
        return state_;
    }

    // Remember current call depth
    size_t current_depth = memory_.call_stack().size();

    state_ = VMState::RUNNING;
    pause_reason_ = PauseReason::NONE;
    pause_requested_ = false;

    // Execute the current command first
    if (!execute_command()) {
        return state_;
    }

    // If we went deeper (call), keep running until we return to same depth
    while (state_ == VMState::RUNNING && memory_.call_stack().size() > current_depth) {
        if (!execute_command()) {
            break;
        }
    }

    if (state_ == VMState::RUNNING) {
        state_ = VMState::PAUSED;
        pause_reason_ = PauseReason::STEP_COMPLETE;
    }

    return state_;
}

void VMEngine::pause() {
    pause_requested_ = true;
}

// ==============================================================================
// State Inspection
// ==============================================================================

const VMCommand* VMEngine::get_command(size_t index) const {
    if (index >= program_.commands.size()) {
        return nullptr;
    }
    return &program_.commands[index];
}

// ==============================================================================
// Breakpoints
// ==============================================================================

void VMEngine::add_breakpoint(size_t command_index) {
    breakpoints_.insert(command_index);
}

void VMEngine::add_function_breakpoint(const std::string& function_name, size_t offset) {
    auto it = program_.function_entry_points.find(function_name);
    if (it != program_.function_entry_points.end()) {
        breakpoints_.insert(it->second + offset);
    }
}

void VMEngine::remove_breakpoint(size_t command_index) {
    breakpoints_.erase(command_index);
}

void VMEngine::clear_breakpoints() {
    breakpoints_.clear();
}

bool VMEngine::has_breakpoint(size_t command_index) const {
    return breakpoints_.count(command_index) > 0;
}

std::vector<size_t> VMEngine::get_breakpoints() const {
    return std::vector<size_t>(breakpoints_.begin(), breakpoints_.end());
}

// ==============================================================================
// Execution Helpers
// ==============================================================================

void VMEngine::initialize_execution() {
    memory_.reset();

    // Determine entry point
    std::string entry = entry_point_;
    if (entry.empty()) {
        // Try Sys.init first (standard bootstrap), then Main.main
        if (program_.function_entry_points.count("Sys.init")) {
            entry = "Sys.init";
        } else if (program_.function_entry_points.count("Main.main")) {
            entry = "Main.main";
        }
    }

    if (!entry.empty()) {
        auto it = program_.function_entry_points.find(entry);
        if (it != program_.function_entry_points.end()) {
            pc_ = it->second;

            // Look up num_locals from the function definition command
            uint16_t num_locals = 0;
            if (pc_ < program_.commands.size()) {
                const VMCommand& cmd = program_.commands[pc_];
                if (std::holds_alternative<FunctionCommand>(cmd)) {
                    num_locals = std::get<FunctionCommand>(cmd).num_locals;
                }
            }

            // Bootstrap: set up a proper call frame on the RAM stack
            // Return address 0 signals halt when the entry function returns
            memory_.push_frame(0, entry, 0, num_locals);
        } else {
            set_error("Entry point function '" + entry + "' not found");
            return;
        }
    } else {
        // No entry function found; start from command 0
        pc_ = 0;
    }

    // Pre-allocate static segments for all source files
    for (const auto& file : program_.source_files) {
        memory_.get_static_base(get_file_basename(file));
    }

    state_ = VMState::PAUSED;
    pause_reason_ = PauseReason::NONE;
}

bool VMEngine::execute_command() {
    // Check for halt: PC past end of program
    if (pc_ >= program_.commands.size()) {
        state_ = VMState::HALTED;
        return false;
    }

    // Check for user-requested pause
    if (pause_requested_) {
        pause_requested_ = false;
        state_ = VMState::PAUSED;
        pause_reason_ = PauseReason::USER_REQUEST;
        return false;
    }

    // Check for breakpoint (but not on the very first instruction after a run/step)
    if (stats_.instructions_executed > 0 && breakpoints_.count(pc_)) {
        state_ = VMState::PAUSED;
        pause_reason_ = PauseReason::BREAKPOINT;
        return false;
    }

    const VMCommand& cmd = program_.commands[pc_];

    try {
        std::visit([this](const auto& command) {
            using T = std::decay_t<decltype(command)>;

            if constexpr (std::is_same_v<T, ArithmeticCommand>) {
                execute_arithmetic(command);
            } else if constexpr (std::is_same_v<T, PushCommand>) {
                execute_push(command);
            } else if constexpr (std::is_same_v<T, PopCommand>) {
                execute_pop(command);
            } else if constexpr (std::is_same_v<T, LabelCommand>) {
                execute_label(command);
            } else if constexpr (std::is_same_v<T, GotoCommand>) {
                execute_goto(command);
            } else if constexpr (std::is_same_v<T, IfGotoCommand>) {
                execute_if_goto(command);
            } else if constexpr (std::is_same_v<T, FunctionCommand>) {
                execute_function(command);
            } else if constexpr (std::is_same_v<T, CallCommand>) {
                execute_call(command);
            } else if constexpr (std::is_same_v<T, ReturnCommand>) {
                execute_return(command);
            }
        }, cmd);

        stats_.instructions_executed++;

    } catch (const N2TError& e) {
        error_message_ = e.what();
        error_location_ = pc_;
        state_ = VMState::ERROR;
        return false;
    } catch (const std::exception& e) {
        error_message_ = std::string("Unexpected error: ") + e.what();
        error_location_ = pc_;
        state_ = VMState::ERROR;
        return false;
    }

    return true;
}

// ==============================================================================
// Command Execution
// ==============================================================================

void VMEngine::execute_arithmetic(const ArithmeticCommand& cmd) {
    stats_.arithmetic_count++;

    switch (cmd.operation) {
        case ArithmeticOp::ADD: {
            int16_t y = static_cast<int16_t>(memory_.pop());
            int16_t x = static_cast<int16_t>(memory_.pop());
            memory_.push(static_cast<Word>(x + y));
            break;
        }
        case ArithmeticOp::SUB: {
            int16_t y = static_cast<int16_t>(memory_.pop());
            int16_t x = static_cast<int16_t>(memory_.pop());
            memory_.push(static_cast<Word>(x - y));
            break;
        }
        case ArithmeticOp::NEG: {
            int16_t y = static_cast<int16_t>(memory_.pop());
            memory_.push(static_cast<Word>(-y));
            break;
        }
        case ArithmeticOp::EQ: {
            Word y = memory_.pop();
            Word x = memory_.pop();
            // True = -1 (0xFFFF), False = 0
            memory_.push(x == y ? 0xFFFF : 0);
            break;
        }
        case ArithmeticOp::GT: {
            int16_t y = static_cast<int16_t>(memory_.pop());
            int16_t x = static_cast<int16_t>(memory_.pop());
            memory_.push(x > y ? 0xFFFF : 0);
            break;
        }
        case ArithmeticOp::LT: {
            int16_t y = static_cast<int16_t>(memory_.pop());
            int16_t x = static_cast<int16_t>(memory_.pop());
            memory_.push(x < y ? 0xFFFF : 0);
            break;
        }
        case ArithmeticOp::AND: {
            Word y = memory_.pop();
            Word x = memory_.pop();
            memory_.push(x & y);
            break;
        }
        case ArithmeticOp::OR: {
            Word y = memory_.pop();
            Word x = memory_.pop();
            memory_.push(x | y);
            break;
        }
        case ArithmeticOp::NOT: {
            Word y = memory_.pop();
            memory_.push(~y);
            break;
        }
    }

    pc_++;
}

void VMEngine::execute_push(const PushCommand& cmd) {
    stats_.push_count++;

    Word value = memory_.read_segment(cmd.segment, cmd.index, cmd.file_name);
    memory_.push(value);

    pc_++;
}

void VMEngine::execute_pop(const PopCommand& cmd) {
    stats_.pop_count++;

    Word value = memory_.pop();
    memory_.write_segment(cmd.segment, cmd.index, value, cmd.file_name);

    pc_++;
}

void VMEngine::execute_label(const LabelCommand& /*cmd*/) {
    // Labels are no-ops at runtime; just advance PC
    pc_++;
}

void VMEngine::execute_goto(const GotoCommand& cmd) {
    pc_ = lookup_label(cmd.label_name);
}

void VMEngine::execute_if_goto(const IfGotoCommand& cmd) {
    Word condition = memory_.pop();
    if (condition != 0) {
        pc_ = lookup_label(cmd.label_name);
    } else {
        pc_++;
    }
}

void VMEngine::execute_function(const FunctionCommand& /*cmd*/) {
    // Function definitions are handled at call time:
    // - push_frame (called by execute_call or bootstrap) sets up the frame
    //   and initializes local variables to 0
    // - At runtime, the function command is a no-op; just advance PC
    pc_++;
}

void VMEngine::execute_call(const CallCommand& cmd) {
    stats_.call_count++;

    // Look up the function entry point
    size_t function_pc = lookup_function(cmd.function_name);

    // The return address is the command after this call
    size_t return_address = pc_ + 1;

    // Push frame saves state and sets up for the called function
    // We pass 0 for num_locals here; the function command will set local count
    // Actually, push_frame needs num_locals to initialize them.
    // We need to look at the function command to get num_locals.
    uint16_t num_locals = 0;
    if (function_pc < program_.commands.size()) {
        const VMCommand& func_cmd = program_.commands[function_pc];
        if (std::holds_alternative<FunctionCommand>(func_cmd)) {
            num_locals = std::get<FunctionCommand>(func_cmd).num_locals;
        }
    }

    memory_.push_frame(return_address, cmd.function_name, cmd.num_args, num_locals);

    // Jump to function
    pc_ = function_pc;
}

void VMEngine::execute_return(const ReturnCommand& /*cmd*/) {
    stats_.return_count++;

    // Pop return value from stack
    Word return_value = memory_.pop();

    // Pop frame and get return address
    size_t return_address = memory_.pop_frame(return_value);

    // Special case: return address 0 means halt (return from top-level)
    if (return_address == 0) {
        state_ = VMState::HALTED;
        return;
    }

    pc_ = return_address;
}

// ==============================================================================
// Lookup Helpers
// ==============================================================================

size_t VMEngine::lookup_label(const std::string& label_name) const {
    // Labels are stored with scoped names: "functionName$labelName"
    // Try scoped lookup first (using current function context)
    std::string current_func = memory_.current_function();
    if (!current_func.empty()) {
        std::string scoped = current_func + "$" + label_name;
        auto it = program_.label_positions.find(scoped);
        if (it != program_.label_positions.end()) {
            return it->second;
        }
    }

    // Fall back to raw label name (for programs without function scoping)
    auto it = program_.label_positions.find(label_name);
    if (it != program_.label_positions.end()) {
        return it->second;
    }

    throw RuntimeError(
        "Undefined label: '" + label_name + "'. "
        "Make sure the label is defined in the current function with 'label " +
        label_name + "'."
    );
}

size_t VMEngine::lookup_function(const std::string& function_name) const {
    auto it = program_.function_entry_points.find(function_name);
    if (it != program_.function_entry_points.end()) {
        return it->second;
    }

    throw RuntimeError(
        "Undefined function: '" + function_name + "'. "
        "Make sure the function is defined with 'function " + function_name +
        " <nLocals>' and the .vm file containing it has been loaded."
    );
}

void VMEngine::set_error(const std::string& message) {
    error_message_ = message;
    error_location_ = pc_;
    state_ = VMState::ERROR;
}

}  // namespace n2t
