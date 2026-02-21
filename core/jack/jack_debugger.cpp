// ==============================================================================
// Jack Debugger - Implementation
// ==============================================================================

#include "jack_debugger.hpp"
#include "error.hpp"
#include <sstream>
#include <algorithm>

namespace n2t {

JackDebugger::JackDebugger() = default;

// ==============================================================================
// Program Loading
// ==============================================================================

void JackDebugger::load(const std::string& vm_source,
                        const std::string& smap_source,
                        const std::string& name) {
    engine_.load_string(vm_source, name);
    source_map_.load_string(smap_source, name + ".smap");
    jack_pause_reason_ = JackPauseReason::NONE;
    jack_stats_.reset();
}

void JackDebugger::load_files(const std::string& vm_path,
                              const std::string& smap_path) {
    engine_.load_file(vm_path);
    source_map_.load_file(smap_path);
    jack_pause_reason_ = JackPauseReason::NONE;
    jack_stats_.reset();
}

void JackDebugger::load_vm(const std::string& vm_source,
                           const std::string& name) {
    engine_.load_string(vm_source, name);
    jack_pause_reason_ = JackPauseReason::NONE;
    jack_stats_.reset();
}

void JackDebugger::load_source_map(const std::string& smap_source,
                                   const std::string& name) {
    source_map_.load_string(smap_source, name);
}

void JackDebugger::set_entry_point(const std::string& function_name) {
    engine_.set_entry_point(function_name);
}

void JackDebugger::reset() {
    engine_.reset();
    jack_pause_reason_ = JackPauseReason::NONE;
    jack_stats_.reset();
    sync_breakpoints();
}

// ==============================================================================
// Execution Control
// ==============================================================================

VMState JackDebugger::step() {
    if (engine_.get_state() == VMState::HALTED ||
        engine_.get_state() == VMState::ERROR) {
        return engine_.get_state();
    }

    // Get current Jack source line
    auto current_source = get_current_source();
    std::string current_file;
    LineNumber current_line = 0;
    if (current_source) {
        current_file = current_source->jack_file;
        current_line = current_source->jack_line;
    }

    uint64_t instr_before = engine_.get_stats().instructions_executed;

    // Step VM commands until we land on a different Jack line
    do {
        VMState state = engine_.step();
        if (state == VMState::HALTED || state == VMState::ERROR) {
            update_stats(instr_before);
            return state;
        }

        // Check if we've moved to a different Jack source line
        auto new_source = get_current_source();
        if (new_source) {
            if (new_source->jack_file != current_file ||
                new_source->jack_line != current_line) {
                break;
            }
        }

        // If no source info, step exactly one VM command
        if (!current_source && !new_source) {
            break;
        }
    } while (true);

    update_stats(instr_before);
    jack_pause_reason_ = JackPauseReason::STEP_COMPLETE;
    return engine_.get_state();
}

VMState JackDebugger::step_over() {
    if (engine_.get_state() == VMState::HALTED ||
        engine_.get_state() == VMState::ERROR) {
        return engine_.get_state();
    }

    size_t initial_depth = engine_.get_call_stack().size();
    auto current_source = get_current_source();
    std::string current_file;
    LineNumber current_line = 0;
    if (current_source) {
        current_file = current_source->jack_file;
        current_line = current_source->jack_line;
    }

    uint64_t instr_before = engine_.get_stats().instructions_executed;

    do {
        VMState state = engine_.step();
        if (state == VMState::HALTED || state == VMState::ERROR) {
            update_stats(instr_before);
            return state;
        }

        size_t current_depth = engine_.get_call_stack().size();

        // If we went deeper (into a call), keep running until we come back
        if (current_depth > initial_depth) {
            continue;
        }

        // If we're at the same or shallower depth, check if the source line changed
        auto new_source = get_current_source();
        if (new_source) {
            if (new_source->jack_file != current_file ||
                new_source->jack_line != current_line) {
                break;
            }
        }

        // If no source info at all, do one step
        if (!current_source && !new_source) {
            break;
        }
    } while (true);

    update_stats(instr_before);
    jack_pause_reason_ = JackPauseReason::STEP_COMPLETE;
    return engine_.get_state();
}

VMState JackDebugger::step_out() {
    if (engine_.get_state() == VMState::HALTED ||
        engine_.get_state() == VMState::ERROR) {
        return engine_.get_state();
    }

    size_t initial_depth = engine_.get_call_stack().size();
    uint64_t instr_before = engine_.get_stats().instructions_executed;

    // Run until call depth decreases
    do {
        VMState state = engine_.step();
        if (state == VMState::HALTED || state == VMState::ERROR) {
            update_stats(instr_before);
            return state;
        }

        size_t current_depth = engine_.get_call_stack().size();
        if (current_depth < initial_depth) {
            // We've returned; now step to the next Jack line
            auto new_source = get_current_source();
            if (new_source) {
                // We're already on a new line in the caller
                break;
            }
            // Otherwise keep stepping until we hit a Jack line
            // (this handles VM commands after return that aren't mapped)
        }
    } while (true);

    update_stats(instr_before);
    jack_pause_reason_ = JackPauseReason::STEP_COMPLETE;
    return engine_.get_state();
}

VMState JackDebugger::run() {
    if (engine_.get_state() == VMState::HALTED ||
        engine_.get_state() == VMState::ERROR) {
        return engine_.get_state();
    }

    sync_breakpoints();
    uint64_t instr_before = engine_.get_stats().instructions_executed;

    VMState state = engine_.run();

    update_stats(instr_before);

    if (state == VMState::PAUSED) {
        if (engine_.get_pause_reason() == PauseReason::BREAKPOINT) {
            jack_pause_reason_ = JackPauseReason::BREAKPOINT;
        } else if (engine_.get_pause_reason() == PauseReason::USER_REQUEST) {
            jack_pause_reason_ = JackPauseReason::USER_REQUEST;
        } else if (engine_.get_pause_reason() == PauseReason::FUNCTION_ENTRY) {
            jack_pause_reason_ = JackPauseReason::FUNCTION_ENTRY;
        } else if (engine_.get_pause_reason() == PauseReason::FUNCTION_EXIT) {
            jack_pause_reason_ = JackPauseReason::FUNCTION_EXIT;
        } else {
            jack_pause_reason_ = JackPauseReason::STEP_COMPLETE;
        }
    }

    return state;
}

VMState JackDebugger::run_for(uint64_t max_instructions) {
    if (engine_.get_state() == VMState::HALTED ||
        engine_.get_state() == VMState::ERROR) {
        return engine_.get_state();
    }

    sync_breakpoints();
    uint64_t instr_before = engine_.get_stats().instructions_executed;

    VMState state = engine_.run_for(max_instructions);

    update_stats(instr_before);

    if (state == VMState::PAUSED &&
        engine_.get_pause_reason() == PauseReason::BREAKPOINT) {
        jack_pause_reason_ = JackPauseReason::BREAKPOINT;
    }

    return state;
}

void JackDebugger::pause() {
    engine_.pause();
}

// ==============================================================================
// State Inspection
// ==============================================================================

VMState JackDebugger::get_state() const {
    return engine_.get_state();
}

bool JackDebugger::is_running() const {
    return engine_.is_running();
}

std::optional<SourceEntry> JackDebugger::get_current_source() const {
    return source_map_.get_entry_for_vm(engine_.get_pc());
}

std::string JackDebugger::get_current_function() const {
    return engine_.get_current_function();
}

std::vector<JackCallFrame> JackDebugger::get_jack_call_stack() const {
    const auto& vm_stack = engine_.get_call_stack();
    std::vector<JackCallFrame> result;
    result.reserve(vm_stack.size());

    for (const auto& frame : vm_stack) {
        JackCallFrame jack_frame;
        jack_frame.function_name = frame.function_name;
        jack_frame.vm_command_index = frame.return_address;

        // Look up Jack source info for the return address
        auto entry = source_map_.get_entry_for_vm(frame.return_address);
        if (entry) {
            jack_frame.jack_file = entry->jack_file;
            jack_frame.jack_line = entry->jack_line;
        }

        result.push_back(jack_frame);
    }

    return result;
}

// ==============================================================================
// Breakpoints
// ==============================================================================

bool JackDebugger::add_breakpoint(const std::string& file, LineNumber line) {
    auto vm_index = source_map_.get_vm_index_for_line(file, line);
    if (!vm_index) {
        return false;
    }

    jack_breakpoints_.insert({file, line});
    engine_.add_breakpoint(*vm_index);
    return true;
}

bool JackDebugger::remove_breakpoint(const std::string& file, LineNumber line) {
    auto it = jack_breakpoints_.find({file, line});
    if (it == jack_breakpoints_.end()) {
        return false;
    }

    jack_breakpoints_.erase(it);

    // Remove corresponding VM breakpoints
    auto vm_indices = source_map_.get_all_vm_indices_for_line(file, line);
    for (auto idx : vm_indices) {
        engine_.remove_breakpoint(idx);
    }
    return true;
}

void JackDebugger::clear_breakpoints() {
    jack_breakpoints_.clear();
    engine_.clear_breakpoints();
}

std::vector<std::pair<std::string, LineNumber>> JackDebugger::get_breakpoints() const {
    return {jack_breakpoints_.begin(), jack_breakpoints_.end()};
}

bool JackDebugger::has_breakpoint(const std::string& file, LineNumber line) const {
    return jack_breakpoints_.count({file, line}) > 0;
}

// ==============================================================================
// Variable Inspection
// ==============================================================================

std::optional<JackVariableValue> JackDebugger::get_variable(
    const std::string& name) const {
    std::string func_name = engine_.get_current_function();
    const FunctionSymbols* symbols = source_map_.get_function_symbols(func_name);
    if (!symbols) {
        return std::nullopt;
    }

    // Search locals, arguments, fields, statics
    auto search = [&](const std::vector<JackVariable>& vars)
        -> std::optional<JackVariableValue> {
        for (const auto& var : vars) {
            if (var.name == name) {
                JackVariableValue val;
                val.name = var.name;
                val.type_name = var.type_name;
                val.kind = var.kind;
                val.index = var.index;
                val.raw_value = read_variable_value(var);
                val.signed_value = static_cast<int16_t>(val.raw_value);
                return val;
            }
        }
        return std::nullopt;
    };

    if (auto v = search(symbols->locals))    return v;
    if (auto v = search(symbols->arguments)) return v;
    if (auto v = search(symbols->fields))    return v;
    if (auto v = search(symbols->statics))   return v;

    return std::nullopt;
}

std::vector<JackVariableValue> JackDebugger::get_all_variables() const {
    std::vector<JackVariableValue> result;

    std::string func_name = engine_.get_current_function();
    const FunctionSymbols* symbols = source_map_.get_function_symbols(func_name);
    if (!symbols) {
        return result;
    }

    auto add_vars = [&](const std::vector<JackVariable>& vars) {
        for (const auto& var : vars) {
            JackVariableValue val;
            val.name = var.name;
            val.type_name = var.type_name;
            val.kind = var.kind;
            val.index = var.index;
            val.raw_value = read_variable_value(var);
            val.signed_value = static_cast<int16_t>(val.raw_value);
            result.push_back(val);
        }
    };

    add_vars(symbols->locals);
    add_vars(symbols->arguments);
    add_vars(symbols->fields);
    add_vars(symbols->statics);

    return result;
}

std::optional<int16_t> JackDebugger::evaluate(const std::string& expr) const {
    // Try integer literal first
    try {
        size_t pos = 0;
        long val = std::stol(expr, &pos);
        if (pos == expr.size()) {
            return static_cast<int16_t>(val);
        }
    } catch (...) {
        // Not an integer literal, try variable
    }

    // Try variable name
    auto var = get_variable(expr);
    if (var) {
        return var->signed_value;
    }

    return std::nullopt;
}

// ==============================================================================
// Object Inspection
// ==============================================================================

InspectedObject JackDebugger::inspect_object(Address address,
                                              const std::string& class_name) const {
    ObjectInspector inspector(engine_.memory(), source_map_);
    return inspector.inspect_object(address, class_name);
}

InspectedObject JackDebugger::inspect_this() const {
    ObjectInspector inspector(engine_.memory(), source_map_);
    return inspector.inspect_this(engine_.get_current_function());
}

InspectedArray JackDebugger::inspect_array(Address address, size_t length) const {
    ObjectInspector inspector(engine_.memory(), source_map_);
    return inspector.inspect_array(address, length);
}

// ==============================================================================
// Statistics
// ==============================================================================

void JackDebugger::reset_stats() {
    jack_stats_.reset();
}

// ==============================================================================
// Internal Helpers
// ==============================================================================

void JackDebugger::sync_breakpoints() {
    engine_.clear_breakpoints();
    for (const auto& bp : jack_breakpoints_) {
        auto vm_indices = source_map_.get_all_vm_indices_for_line(
            bp.first, bp.second);
        for (auto idx : vm_indices) {
            engine_.add_breakpoint(idx);
        }
    }
}

void JackDebugger::update_stats(uint64_t instructions_before) {
    uint64_t executed = engine_.get_stats().instructions_executed - instructions_before;
    jack_stats_.total_vm_instructions += executed;

    // Track per-function stats
    std::string func = engine_.get_current_function();
    if (!func.empty()) {
        jack_stats_.function_instruction_counts[func] += executed;
    }

    // Track call counts from VM stats
    if (engine_.get_stats().call_count > 0) {
        // We can't precisely attribute which function was called here,
        // but the current function's call count can be approximated
    }
}

bool JackDebugger::is_at_jack_breakpoint() const {
    auto source = get_current_source();
    if (!source) return false;
    return jack_breakpoints_.count({source->jack_file, source->jack_line}) > 0;
}

Word JackDebugger::read_variable_value(const JackVariable& var) const {
    switch (var.kind) {
        case JackVarKind::LOCAL:
            return engine_.get_segment(SegmentType::LOCAL, var.index);
        case JackVarKind::ARGUMENT:
            return engine_.get_segment(SegmentType::ARGUMENT, var.index);
        case JackVarKind::FIELD: {
            // Fields are accessed via THIS segment
            Address this_addr = engine_.read_ram(VMAddress::THIS);
            return engine_.read_ram(
                static_cast<Address>(this_addr + var.index));
        }
        case JackVarKind::STATIC:
            return engine_.get_segment(SegmentType::STATIC, var.index);
    }
    return 0;
}

}  // namespace n2t
