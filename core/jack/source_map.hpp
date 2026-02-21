// ==============================================================================
// Jack Source Map
// ==============================================================================
// Parses .smap files and provides bidirectional mapping between Jack source
// locations and VM command indices. Also stores symbol tables (variable
// names/types per function) and class layouts (field names/order).
// ==============================================================================

#ifndef NAND2TETRIS_JACK_SOURCE_MAP_HPP
#define NAND2TETRIS_JACK_SOURCE_MAP_HPP

#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>

namespace n2t {

// ==============================================================================
// Jack Variable Types
// ==============================================================================

enum class JackVarKind {
    LOCAL,
    ARGUMENT,
    FIELD,
    STATIC
};

struct JackVariable {
    std::string name;
    std::string type_name;
    JackVarKind kind;
    uint16_t index;
};

struct FunctionSymbols {
    std::string function_name;
    std::string class_name;
    std::vector<JackVariable> locals;
    std::vector<JackVariable> arguments;
    std::vector<JackVariable> fields;
    std::vector<JackVariable> statics;
};

// ==============================================================================
// Source Map Entries
// ==============================================================================

struct SourceEntry {
    std::string jack_file;
    LineNumber jack_line;
    size_t vm_command_index;
    std::string function_name;
};

struct ClassLayout {
    std::string class_name;
    std::vector<JackVariable> fields;
};

// ==============================================================================
// Source Map Class
// ==============================================================================

class SourceMap {
public:
    SourceMap() = default;

    // Load source map from a .smap file
    void load_file(const std::string& file_path);

    // Load source map from a string
    void load_string(const std::string& source, const std::string& name = "<smap>");

    // Clear all data
    void clear();

    // Forward lookup: VM command index -> source entry
    std::optional<SourceEntry> get_entry_for_vm(size_t vm_index) const;

    // Reverse lookup: (file, line) -> VM command index
    std::optional<size_t> get_vm_index_for_line(const std::string& file,
                                                 LineNumber line) const;

    // Get all VM indices that map to a given file and line
    std::vector<size_t> get_all_vm_indices_for_line(const std::string& file,
                                                     LineNumber line) const;

    // Get function symbols by function name
    const FunctionSymbols* get_function_symbols(const std::string& function_name) const;

    // Get class layout by class name
    const ClassLayout* get_class_layout(const std::string& class_name) const;

    // Get all source entries
    const std::vector<SourceEntry>& entries() const { return entries_; }

    // Check if loaded
    bool empty() const { return entries_.empty(); }

    // Get all function names
    std::vector<std::string> get_function_names() const;

    // Get all class names
    std::vector<std::string> get_class_names() const;

private:
    std::vector<SourceEntry> entries_;

    // VM index -> entry index (O(1) forward lookup)
    std::unordered_map<size_t, size_t> vm_to_entry_;

    // (file, line) -> entry index (breakpoint lookup)
    std::map<std::pair<std::string, LineNumber>, size_t> line_to_entry_;

    // Function symbol tables
    std::unordered_map<std::string, FunctionSymbols> function_symbols_;

    // Class layouts
    std::unordered_map<std::string, ClassLayout> class_layouts_;

    // Current parsing state
    std::string current_func_name_;

    void parse_line(const std::string& line, LineNumber line_num,
                    const std::string& source_name);
    void parse_map_line(const std::string& line, LineNumber line_num,
                        const std::string& source_name);
    void parse_func_line(const std::string& line, LineNumber line_num,
                         const std::string& source_name);
    void parse_var_line(const std::string& line, LineNumber line_num,
                        const std::string& source_name);
    void parse_class_line(const std::string& line, LineNumber line_num,
                          const std::string& source_name);
    void parse_field_line(const std::string& line, LineNumber line_num,
                          const std::string& source_name);

    // Current class being parsed
    std::string current_class_name_;
};

}  // namespace n2t

#endif  // NAND2TETRIS_JACK_SOURCE_MAP_HPP
