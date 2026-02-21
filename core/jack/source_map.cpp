// ==============================================================================
// Jack Source Map - Implementation
// ==============================================================================

#include "source_map.hpp"
#include "error.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace n2t {

// ==============================================================================
// Loading
// ==============================================================================

void SourceMap::load_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw FileError(file_path, "Cannot open source map file");
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    load_string(content, file_path);
}

void SourceMap::load_string(const std::string& source, const std::string& name) {
    clear();

    std::istringstream stream(source);
    std::string line;
    LineNumber line_num = 0;

    while (std::getline(stream, line)) {
        line_num++;

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        parse_line(line, line_num, name);
    }
}

void SourceMap::clear() {
    entries_.clear();
    vm_to_entry_.clear();
    line_to_entry_.clear();
    function_symbols_.clear();
    class_layouts_.clear();
    current_func_name_.clear();
    current_class_name_.clear();
}

// ==============================================================================
// Query Methods
// ==============================================================================

std::optional<SourceEntry> SourceMap::get_entry_for_vm(size_t vm_index) const {
    auto it = vm_to_entry_.find(vm_index);
    if (it == vm_to_entry_.end()) {
        return std::nullopt;
    }
    return entries_[it->second];
}

std::optional<size_t> SourceMap::get_vm_index_for_line(
    const std::string& file, LineNumber line) const {
    auto it = line_to_entry_.find({file, line});
    if (it == line_to_entry_.end()) {
        return std::nullopt;
    }
    return entries_[it->second].vm_command_index;
}

std::vector<size_t> SourceMap::get_all_vm_indices_for_line(
    const std::string& file, LineNumber line) const {
    std::vector<size_t> result;
    for (const auto& entry : entries_) {
        if (entry.jack_file == file && entry.jack_line == line) {
            result.push_back(entry.vm_command_index);
        }
    }
    return result;
}

const FunctionSymbols* SourceMap::get_function_symbols(
    const std::string& function_name) const {
    auto it = function_symbols_.find(function_name);
    if (it == function_symbols_.end()) {
        return nullptr;
    }
    return &it->second;
}

const ClassLayout* SourceMap::get_class_layout(const std::string& class_name) const {
    auto it = class_layouts_.find(class_name);
    if (it == class_layouts_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> SourceMap::get_function_names() const {
    std::vector<std::string> names;
    names.reserve(function_symbols_.size());
    for (const auto& pair : function_symbols_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> SourceMap::get_class_names() const {
    std::vector<std::string> names;
    names.reserve(class_layouts_.size());
    for (const auto& pair : class_layouts_) {
        names.push_back(pair.first);
    }
    return names;
}

// ==============================================================================
// Parsing
// ==============================================================================

void SourceMap::parse_line(const std::string& line, LineNumber line_num,
                           const std::string& source_name) {
    // Determine line type by prefix
    if (line.substr(0, 4) == "MAP ") {
        parse_map_line(line, line_num, source_name);
    } else if (line.substr(0, 5) == "FUNC ") {
        parse_func_line(line, line_num, source_name);
    } else if (line.substr(0, 4) == "VAR ") {
        parse_var_line(line, line_num, source_name);
    } else if (line.substr(0, 6) == "CLASS ") {
        parse_class_line(line, line_num, source_name);
    } else if (line.substr(0, 6) == "FIELD ") {
        parse_field_line(line, line_num, source_name);
    } else {
        throw ParseError(source_name, line_num,
                          "Unknown source map directive: '" + line + "'");
    }
}

void SourceMap::parse_map_line(const std::string& line, LineNumber line_num,
                               const std::string& source_name) {
    // Format: MAP Main:10 -> 45 [Main.main]
    std::istringstream iss(line);
    std::string keyword;
    std::string file_and_line;
    std::string arrow;
    size_t vm_index = 0;
    std::string func_bracket;

    iss >> keyword >> file_and_line >> arrow >> vm_index;

    if (keyword != "MAP" || arrow != "->") {
        throw ParseError(source_name, line_num,
                          "Invalid MAP format: '" + line + "'");
    }

    // Parse file:line
    auto colon_pos = file_and_line.find(':');
    if (colon_pos == std::string::npos) {
        throw ParseError(source_name, line_num,
                          "Invalid MAP source location: '" + file_and_line + "'");
    }

    std::string jack_file = file_and_line.substr(0, colon_pos);
    LineNumber jack_line = 0;
    try {
        jack_line = static_cast<LineNumber>(
            std::stoul(file_and_line.substr(colon_pos + 1)));
    } catch (...) {
        throw ParseError(source_name, line_num,
                          "Invalid line number in MAP: '" + file_and_line + "'");
    }

    // Parse optional [function_name]
    std::string function_name;
    if (iss >> func_bracket) {
        if (func_bracket.front() == '[' && func_bracket.back() == ']') {
            function_name = func_bracket.substr(1, func_bracket.size() - 2);
        }
    }

    SourceEntry entry;
    entry.jack_file = jack_file;
    entry.jack_line = jack_line;
    entry.vm_command_index = vm_index;
    entry.function_name = function_name;

    size_t entry_index = entries_.size();
    entries_.push_back(entry);
    vm_to_entry_[vm_index] = entry_index;

    // For reverse lookup, store the first VM index for each (file, line) pair
    auto key = std::make_pair(jack_file, jack_line);
    if (line_to_entry_.find(key) == line_to_entry_.end()) {
        line_to_entry_[key] = entry_index;
    }
}

void SourceMap::parse_func_line(const std::string& line, LineNumber line_num,
                                const std::string& source_name) {
    // Format: FUNC Main.main
    std::istringstream iss(line);
    std::string keyword;
    std::string func_name;

    iss >> keyword >> func_name;

    if (keyword != "FUNC" || func_name.empty()) {
        throw ParseError(source_name, line_num,
                          "Invalid FUNC format: '" + line + "'");
    }

    current_func_name_ = func_name;

    // Extract class name from function name (e.g., "Main" from "Main.main")
    std::string class_name;
    auto dot_pos = func_name.find('.');
    if (dot_pos != std::string::npos) {
        class_name = func_name.substr(0, dot_pos);
    }

    FunctionSymbols& symbols = function_symbols_[func_name];
    symbols.function_name = func_name;
    symbols.class_name = class_name;
}

void SourceMap::parse_var_line(const std::string& line, LineNumber line_num,
                               const std::string& source_name) {
    // Format: VAR local int sum 0
    std::istringstream iss(line);
    std::string keyword;
    std::string kind_str;
    std::string type_name;
    std::string var_name;
    uint16_t index = 0;

    iss >> keyword >> kind_str >> type_name >> var_name >> index;

    if (keyword != "VAR" || kind_str.empty() || type_name.empty() ||
        var_name.empty()) {
        throw ParseError(source_name, line_num,
                          "Invalid VAR format: '" + line + "'");
    }

    if (current_func_name_.empty()) {
        throw ParseError(source_name, line_num,
                          "VAR without preceding FUNC");
    }

    JackVarKind kind;
    if (kind_str == "local") {
        kind = JackVarKind::LOCAL;
    } else if (kind_str == "argument") {
        kind = JackVarKind::ARGUMENT;
    } else if (kind_str == "field") {
        kind = JackVarKind::FIELD;
    } else if (kind_str == "static") {
        kind = JackVarKind::STATIC;
    } else {
        throw ParseError(source_name, line_num,
                          "Invalid variable kind: '" + kind_str + "'");
    }

    JackVariable var;
    var.name = var_name;
    var.type_name = type_name;
    var.kind = kind;
    var.index = index;

    FunctionSymbols& symbols = function_symbols_[current_func_name_];
    switch (kind) {
        case JackVarKind::LOCAL:    symbols.locals.push_back(var);    break;
        case JackVarKind::ARGUMENT: symbols.arguments.push_back(var); break;
        case JackVarKind::FIELD:    symbols.fields.push_back(var);    break;
        case JackVarKind::STATIC:   symbols.statics.push_back(var);   break;
    }
}

void SourceMap::parse_class_line(const std::string& line, LineNumber line_num,
                                 const std::string& source_name) {
    // Format: CLASS Point
    std::istringstream iss(line);
    std::string keyword;
    std::string class_name;

    iss >> keyword >> class_name;

    if (keyword != "CLASS" || class_name.empty()) {
        throw ParseError(source_name, line_num,
                          "Invalid CLASS format: '" + line + "'");
    }

    current_class_name_ = class_name;
    class_layouts_[class_name].class_name = class_name;
}

void SourceMap::parse_field_line(const std::string& line, LineNumber line_num,
                                 const std::string& source_name) {
    // Format: FIELD int x
    std::istringstream iss(line);
    std::string keyword;
    std::string type_name;
    std::string field_name;

    iss >> keyword >> type_name >> field_name;

    if (keyword != "FIELD" || type_name.empty() || field_name.empty()) {
        throw ParseError(source_name, line_num,
                          "Invalid FIELD format: '" + line + "'");
    }

    if (current_class_name_.empty()) {
        throw ParseError(source_name, line_num,
                          "FIELD without preceding CLASS");
    }

    ClassLayout& layout = class_layouts_[current_class_name_];
    JackVariable field;
    field.name = field_name;
    field.type_name = type_name;
    field.kind = JackVarKind::FIELD;
    field.index = static_cast<uint16_t>(layout.fields.size());

    layout.fields.push_back(field);
}

}  // namespace n2t
