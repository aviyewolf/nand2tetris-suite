// ==============================================================================
// Auto Source Map Generator - Implementation
// ==============================================================================

#include "auto_source_map.hpp"
#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace n2t {

// ==============================================================================
// Internal helpers
// ==============================================================================

namespace {

// Find the VM command range for each function: [start, end) in command index space.
struct FunctionRange {
    std::string name;
    size_t start;   // index of the FunctionCommand
    size_t end;     // index past the last command of this function
};

std::vector<FunctionRange> find_function_ranges(const std::vector<VMCommand>& commands) {
    std::vector<FunctionRange> ranges;

    for (size_t i = 0; i < commands.size(); ++i) {
        if (auto* fc = std::get_if<FunctionCommand>(&commands[i])) {
            if (!ranges.empty()) {
                ranges.back().end = i;
            }
            ranges.push_back({fc->function_name, i, 0});
        }
    }

    if (!ranges.empty()) {
        ranges.back().end = commands.size();
    }

    return ranges;
}

// Identify VM statement boundaries within a function's command range.
// Returns a list of "statement start" command indices.
// Strategy: a new statement starts:
//   - At the first command of the function (after the FunctionCommand itself)
//   - After each "pop" to local/static/this/that/pointer (end of let)
//   - After each "pop temp 0" that follows a "call" (end of do)
//   - At each "return" command
//   - At each label command (while/if boundaries)
//
// We simplify: partition commands into statements by looking for "end of
// statement" markers, then map each group to the Nth Jack statement.

struct StatementGroup {
    size_t start_cmd;   // first VM command in this statement
    size_t end_cmd;     // one past the last VM command
};

std::vector<StatementGroup> segment_vm_statements(
    const std::vector<VMCommand>& commands, size_t range_start, size_t range_end) {

    std::vector<StatementGroup> groups;
    if (range_start >= range_end) return groups;

    // Skip the FunctionCommand itself
    size_t first_cmd = range_start + 1;
    if (first_cmd >= range_end) return groups;

    size_t stmt_start = first_cmd;

    for (size_t i = first_cmd; i < range_end; ++i) {
        bool is_stmt_end = false;

        if (std::holds_alternative<ReturnCommand>(commands[i])) {
            // Return is a complete statement
            is_stmt_end = true;
        } else if (auto* pop = std::get_if<PopCommand>(&commands[i])) {
            // pop to temp 0: could be end of a 'do' statement (discard return value)
            if (pop->segment == SegmentType::TEMP && pop->index == 0) {
                is_stmt_end = true;
            }
            // pop to local/static/this/that/pointer: end of 'let' statement
            else if (pop->segment == SegmentType::LOCAL ||
                     pop->segment == SegmentType::STATIC ||
                     pop->segment == SegmentType::THIS ||
                     pop->segment == SegmentType::THAT ||
                     pop->segment == SegmentType::POINTER) {
                // Check if next command is NOT an arithmetic/push that continues
                // an array-indexed let (pop that N followed by pop temp 0 pattern).
                // Simple heuristic: if the NEXT command is also a pop to the same
                // segment or is a label/return/function, this is a statement end.
                // Actually, simpler: let statements end with a pop to a segment.
                // The array case: let a[expr] = expr generates:
                //   ... push that 0 / pop temp 0 / pop pointer 1 / push temp 0 / pop that 0
                // So "pop that 0" after "push temp 0" is the true end.
                // For simplicity, we mark each pop (non-temp) as a possible statement end.
                is_stmt_end = true;
            }
        }

        if (is_stmt_end) {
            groups.push_back({stmt_start, i + 1});
            stmt_start = i + 1;
        }
    }

    // Any remaining commands form a trailing group
    if (stmt_start < range_end) {
        groups.push_back({stmt_start, range_end});
    }

    return groups;
}

// Build the .smap format string from class info and VM commands
std::string build_smap_string(
    const std::vector<JackClassInfo>& classes,
    const std::vector<VMCommand>& commands) {

    // Build function ranges from VM
    auto func_ranges = find_function_ranges(commands);

    // Build a map from qualified function name to its range
    std::unordered_map<std::string, FunctionRange> range_map;
    for (const auto& r : func_ranges) {
        range_map[r.name] = r;
    }

    // Build a map from qualified function name to its Jack info
    std::unordered_map<std::string, const JackSubroutineInfo*> sub_map;
    std::unordered_map<std::string, const JackClassInfo*> sub_class_map;
    for (const auto& cls : classes) {
        for (const auto& sub : cls.subroutines) {
            sub_map[sub.full_name] = &sub;
            sub_class_map[sub.full_name] = &cls;
        }
    }

    std::ostringstream out;
    out << "# Auto-generated source map\n\n";

    // === MAP entries ===
    // For each function that exists in both Jack and VM, create line mappings
    for (const auto& range : func_ranges) {
        auto sub_it = sub_map.find(range.name);
        if (sub_it == sub_map.end()) continue;

        const auto& sub = *sub_it->second;
        const auto& cls = *sub_class_map.at(range.name);

        // Extract the base filename without extension for the jack_file reference
        std::string jack_file = cls.filename;
        // Strip path — use just the filename
        auto slash_pos = jack_file.find_last_of("/\\");
        if (slash_pos != std::string::npos) {
            jack_file = jack_file.substr(slash_pos + 1);
        }
        // Strip .jack extension to get the class file reference
        auto dot_pos = jack_file.rfind(".jack");
        if (dot_pos != std::string::npos) {
            jack_file = jack_file.substr(0, dot_pos);
        }

        // Map the function declaration line to the FunctionCommand
        out << "MAP " << jack_file << ":" << sub.decl_line
            << " -> " << range.start << " [" << range.name << "]\n";

        // Segment VM commands into statement groups
        auto vm_stmts = segment_vm_statements(commands, range.start, range.end);

        // Map each VM statement group to corresponding Jack statement
        if (!sub.statements.empty() && !vm_stmts.empty()) {
            size_t jack_count = sub.statements.size();
            size_t vm_count = vm_stmts.size();

            if (jack_count == vm_count) {
                // Perfect match: map each VM group to its Jack statement line
                for (size_t i = 0; i < jack_count; ++i) {
                    out << "MAP " << jack_file << ":" << sub.statements[i].second
                        << " -> " << vm_stmts[i].start_cmd
                        << " [" << range.name << "]\n";
                }
            } else {
                // Count mismatch: use proportional mapping as best effort.
                // Map the first command of each VM group to the nearest Jack statement.
                for (size_t vi = 0; vi < vm_count; ++vi) {
                    // Map vi proportionally to a Jack statement index
                    size_t ji = (vi * jack_count) / vm_count;
                    if (ji >= jack_count) ji = jack_count - 1;
                    out << "MAP " << jack_file << ":" << sub.statements[ji].second
                        << " -> " << vm_stmts[vi].start_cmd
                        << " [" << range.name << "]\n";
                }
            }
        } else if (sub.statements.empty() && !vm_stmts.empty()) {
            // No Jack statements found (parser couldn't extract them)
            // Map everything to the declaration line
            for (const auto& g : vm_stmts) {
                out << "MAP " << jack_file << ":" << sub.decl_line
                    << " -> " << g.start_cmd
                    << " [" << range.name << "]\n";
            }
        }
    }

    out << "\n";

    // === FUNC / VAR entries ===
    for (const auto& cls : classes) {
        for (const auto& sub : cls.subroutines) {
            out << "FUNC " << sub.full_name << "\n";

            // Locals
            for (const auto& v : sub.locals) {
                out << "VAR local " << v.type_name << " " << v.name
                    << " " << v.index << "\n";
            }

            // Arguments
            for (const auto& v : sub.parameters) {
                out << "VAR argument " << v.type_name << " " << v.name
                    << " " << v.index << "\n";
            }

            // Fields (from the class)
            for (const auto& v : cls.fields) {
                out << "VAR field " << v.type_name << " " << v.name
                    << " " << v.index << "\n";
            }

            // Statics (from the class)
            for (const auto& v : cls.statics) {
                out << "VAR static " << v.type_name << " " << v.name
                    << " " << v.index << "\n";
            }
        }
    }

    out << "\n";

    // === CLASS / FIELD entries ===
    for (const auto& cls : classes) {
        if (!cls.fields.empty()) {
            out << "CLASS " << cls.name << "\n";
            for (const auto& f : cls.fields) {
                out << "FIELD " << f.type_name << " " << f.name << "\n";
            }
        }
    }

    return out.str();
}

}  // anonymous namespace

// ==============================================================================
// Public API
// ==============================================================================

SourceMap generate_source_map(const std::vector<JackClassInfo>& classes,
                              const std::vector<VMCommand>& commands) {
    std::string smap_text = build_smap_string(classes, commands);

    SourceMap map;
    map.load_string(smap_text, "<auto>");
    return map;
}

}  // namespace n2t
