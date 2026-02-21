// ==============================================================================
// Jack Debugger
// ==============================================================================
// Source-level debugging wrapper around VMEngine. Translates between Jack
// source locations and VM command indices, enabling students to debug at the
// Jack language level rather than raw VM commands.
// ==============================================================================

#ifndef NAND2TETRIS_JACK_DEBUGGER_HPP
#define NAND2TETRIS_JACK_DEBUGGER_HPP

#include "source_map.hpp"
#include "object_inspector.hpp"
#include "vm_engine.hpp"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <optional>

namespace n2t {

// ==============================================================================
// Jack Debugger Types
// ==============================================================================

enum class JackPauseReason {
    NONE,
    STEP_COMPLETE,
    BREAKPOINT,
    FUNCTION_ENTRY,
    FUNCTION_EXIT,
    USER_REQUEST
};

struct JackCallFrame {
    std::string function_name;
    std::string jack_file;
    LineNumber jack_line;
    size_t vm_command_index;
};

struct JackStats {
    uint64_t total_vm_instructions = 0;
    std::unordered_map<std::string, uint64_t> function_call_counts;
    std::unordered_map<std::string, uint64_t> function_instruction_counts;

    void reset() {
        total_vm_instructions = 0;
        function_call_counts.clear();
        function_instruction_counts.clear();
    }
};

struct JackVariableValue {
    std::string name;
    std::string type_name;
    JackVarKind kind;
    uint16_t index;
    Word raw_value;
    int16_t signed_value;
};

// ==============================================================================
// Jack Debugger Class
// ==============================================================================

class JackDebugger {
public:
    JackDebugger();

    // =========================================================================
    // Program Loading
    // =========================================================================

    // Load VM program and source map
    void load(const std::string& vm_source, const std::string& smap_source,
              const std::string& name = "<string>");

    // Load from files
    void load_files(const std::string& vm_path, const std::string& smap_path);

    // Load VM source only (debugging without source map)
    void load_vm(const std::string& vm_source,
                 const std::string& name = "<string>");

    // Load source map separately
    void load_source_map(const std::string& smap_source,
                         const std::string& name = "<smap>");

    // Set entry point
    void set_entry_point(const std::string& function_name);

    // Reset to initial state
    void reset();

    // =========================================================================
    // Execution Control (Jack-level stepping)
    // =========================================================================

    // Step to next Jack source line
    VMState step();

    // Step over function calls (stay at same call depth)
    VMState step_over();

    // Step out of current function
    VMState step_out();

    // Run until breakpoint, halt, or error
    VMState run();

    // Run for max VM instructions
    VMState run_for(uint64_t max_instructions);

    // Pause execution
    void pause();

    // =========================================================================
    // State Inspection
    // =========================================================================

    VMState get_state() const;
    JackPauseReason get_pause_reason() const { return jack_pause_reason_; }
    bool is_running() const;

    // Current Jack source location
    std::optional<SourceEntry> get_current_source() const;

    // Current function name
    std::string get_current_function() const;

    // Jack-level call stack
    std::vector<JackCallFrame> get_jack_call_stack() const;

    // =========================================================================
    // Breakpoints (Jack-level)
    // =========================================================================

    // Add breakpoint at Jack source line
    bool add_breakpoint(const std::string& file, LineNumber line);

    // Remove breakpoint at Jack source line
    bool remove_breakpoint(const std::string& file, LineNumber line);

    // Clear all Jack breakpoints
    void clear_breakpoints();

    // Get all Jack breakpoints as (file, line) pairs
    std::vector<std::pair<std::string, LineNumber>> get_breakpoints() const;

    // Check if there's a breakpoint at a line
    bool has_breakpoint(const std::string& file, LineNumber line) const;

    // =========================================================================
    // Variable Inspection
    // =========================================================================

    // Get value of a variable by name in current scope
    std::optional<JackVariableValue> get_variable(const std::string& name) const;

    // Get all variables in current scope
    std::vector<JackVariableValue> get_all_variables() const;

    // Evaluate a simple expression (integer literal or variable name)
    std::optional<int16_t> evaluate(const std::string& expr) const;

    // =========================================================================
    // Object Inspection
    // =========================================================================

    // Inspect an object at a heap address
    InspectedObject inspect_object(Address address,
                                   const std::string& class_name) const;

    // Inspect the current 'this' object
    InspectedObject inspect_this() const;

    // Inspect an array at a heap address
    InspectedArray inspect_array(Address address, size_t length) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    const JackStats& get_stats() const { return jack_stats_; }
    void reset_stats();

    // =========================================================================
    // Direct VM Access (for advanced users)
    // =========================================================================

    VMEngine& engine() { return engine_; }
    const VMEngine& engine() const { return engine_; }
    const SourceMap& source_map() const { return source_map_; }

private:
    VMEngine engine_;
    SourceMap source_map_;
    JackPauseReason jack_pause_reason_ = JackPauseReason::NONE;
    JackStats jack_stats_;

    // Jack breakpoints stored as (file, line) pairs
    std::set<std::pair<std::string, LineNumber>> jack_breakpoints_;

    // Sync Jack breakpoints to VM engine breakpoints
    void sync_breakpoints();

    // Update stats after VM execution
    void update_stats(uint64_t instructions_before);

    // Check if VM PC is at a Jack breakpoint
    bool is_at_jack_breakpoint() const;

    // Read a variable value from VM memory
    Word read_variable_value(const JackVariable& var) const;
};

}  // namespace n2t

#endif  // NAND2TETRIS_JACK_DEBUGGER_HPP
