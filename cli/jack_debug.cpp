// ==============================================================================
// jack_debug — Jack Debugger CLI
// ==============================================================================
// Batch:       jack_debug --run Prog.vm Prog.smap [-n 10000]
// Interactive: jack_debug Prog.vm Prog.smap
// ==============================================================================

#include "jack_debugger.hpp"
#include "jack_declaration_parser.hpp"
#include "auto_source_map.hpp"
#include "object_inspector.hpp"
#include "error.hpp"
#include "line_editor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <iomanip>

using namespace n2t;

static void print_usage() {
    std::cout << "Usage:\n"
              << "  jack_debug --run Prog.vm Prog.smap [-n <max>]   Run in batch mode\n"
              << "  jack_debug Prog.vm Prog.smap                     Interactive REPL\n"
              << "  jack_debug Prog.vm Main.jack [More.jack ...]     Auto source map from .jack files\n"
              << "  jack_debug --run Prog.vm Main.jack [-n <max>]    Batch mode with .jack files\n"
              << "  jack_debug --help                                 Show this help\n";
}

static bool ends_with(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw FileError(path, "Cannot open file");
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

static void load_with_jack_sources(JackDebugger& dbg, const std::string& vm_path,
                                   const std::vector<std::string>& jack_paths) {
    std::string vm_source = read_file(vm_path);

    std::vector<std::pair<std::string, std::string>> jack_sources;
    for (const auto& path : jack_paths) {
        std::string source = read_file(path);
        // Extract just the filename for display
        std::string filename = path;
        auto slash = filename.find_last_of("/\\");
        if (slash != std::string::npos) filename = filename.substr(slash + 1);
        jack_sources.push_back({filename, source});
    }

    dbg.load_jack(jack_sources, vm_source, vm_path);
}

static std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) args.push_back(token);
    return args;
}

static void print_state(VMState state, const JackDebugger& dbg) {
    switch (state) {
        case VMState::READY:   std::cout << "[READY]\n"; break;
        case VMState::RUNNING: std::cout << "[RUNNING]\n"; break;
        case VMState::PAUSED:
            std::cout << "[PAUSED]";
            switch (dbg.get_pause_reason()) {
                case JackPauseReason::BREAKPOINT:     std::cout << " breakpoint"; break;
                case JackPauseReason::FUNCTION_ENTRY:  std::cout << " function entry"; break;
                case JackPauseReason::FUNCTION_EXIT:   std::cout << " function exit"; break;
                default: break;
            }
            std::cout << "\n";
            break;
        case VMState::HALTED:  std::cout << "[HALTED]\n"; break;
        case VMState::ERROR:   std::cout << "[ERROR] " << dbg.engine().get_error_message() << "\n"; break;
    }
}

static void print_where(const JackDebugger& dbg) {
    auto src = dbg.get_current_source();
    if (src) {
        std::cout << "  " << src->jack_file << ":" << src->jack_line
                  << " in " << src->function_name << "\n";
    } else {
        std::cout << "  (no source mapping at VM cmd " << dbg.engine().get_pc() << ")\n";
    }
}

static void print_variable(const JackVariableValue& v) {
    const char* kind_str = "???";
    switch (v.kind) {
        case JackVarKind::LOCAL:    kind_str = "local"; break;
        case JackVarKind::ARGUMENT: kind_str = "arg"; break;
        case JackVarKind::FIELD:    kind_str = "field"; break;
        case JackVarKind::STATIC:   kind_str = "static"; break;
    }
    std::cout << "  " << kind_str << " " << v.type_name << " " << v.name
              << " = " << v.signed_value
              << " (0x" << std::hex << std::setfill('0') << std::setw(4) << v.raw_value << std::dec << ")\n";
}

static void print_stats(const JackDebugger& dbg) {
    const auto& s = dbg.get_stats();
    std::cout << "  Total VM instructions: " << s.total_vm_instructions << "\n";
    if (!s.function_call_counts.empty()) {
        std::cout << "  Function call counts:\n";
        for (const auto& [name, count] : s.function_call_counts) {
            std::cout << "    " << name << ": " << count << " calls";
            auto it = s.function_instruction_counts.find(name);
            if (it != s.function_instruction_counts.end())
                std::cout << ", " << it->second << " instructions";
            std::cout << "\n";
        }
    }
}

static int batch_mode(const std::string& vm_path, const std::string& smap_path,
                      const std::vector<std::string>& jack_paths, uint64_t max_instr) {
    JackDebugger dbg;
    try {
        if (!jack_paths.empty()) {
            load_with_jack_sources(dbg, vm_path, jack_paths);
        } else {
            dbg.load_files(vm_path, smap_path);
        }
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    VMState state = (max_instr > 0) ? dbg.run_for(max_instr) : dbg.run();

    print_state(state, dbg);
    print_where(dbg);
    std::cout << "\n";
    print_stats(dbg);

    return (state == VMState::ERROR) ? 1 : 0;
}

static void interactive_mode(const std::string& vm_path, const std::string& smap_path,
                             const std::vector<std::string>& jack_paths) {
    JackDebugger dbg;
    try {
        if (!jack_paths.empty()) {
            load_with_jack_sources(dbg, vm_path, jack_paths);
        } else {
            dbg.load_files(vm_path, smap_path);
        }
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "Jack Debugger — " << vm_path << "\n";
    if (!jack_paths.empty()) {
        std::cout << "Source: " << jack_paths.size() << " .jack file(s) (auto source map)\n";
    } else {
        std::cout << "Source map: " << smap_path << "\n";
    }
    std::cout << "Type 'help' for commands.\n\n";

    LineEditor editor("> ");
    std::string line;
    while (true) {
        auto result = editor.read_line();
        if (!result) break;
        line = *result;

        auto args = split_args(line);
        if (args.empty()) continue;

        const auto& cmd = args[0];

        if (cmd == "quit" || cmd == "q") {
            break;
        } else if (cmd == "help" || cmd == "h") {
            std::cout << "Commands:\n"
                      << "  step, s              Step to next Jack source line\n"
                      << "  over, n              Step over function call\n"
                      << "  out                  Step out of function\n"
                      << "  run, r               Run until halt/breakpoint\n"
                      << "  run N                Run for N VM instructions\n"
                      << "  where                Show current Jack source location\n"
                      << "  vars                 Show all variables in scope\n"
                      << "  var <name>           Inspect specific variable\n"
                      << "  eval <expr>          Evaluate expression\n"
                      << "  inspect <addr> <cls> Inspect heap object\n"
                      << "  array <addr> <len>   Inspect array\n"
                      << "  this                 Inspect current 'this' object\n"
                      << "  calls                Jack-level call stack\n"
                      << "  break <file> <line>  Set Jack breakpoint\n"
                      << "  clear <file> <line>  Clear breakpoint\n"
                      << "  breaks               List breakpoints\n"
                      << "  stats                Profiling statistics\n"
                      << "  reset                Reset debugger\n"
                      << "  quit, q              Exit\n";
        } else if (cmd == "step" || cmd == "s") {
            VMState state = dbg.step();
            print_where(dbg);
            if (state != VMState::PAUSED)
                print_state(state, dbg);
        } else if (cmd == "over" || cmd == "n") {
            VMState state = dbg.step_over();
            print_where(dbg);
            if (state != VMState::PAUSED)
                print_state(state, dbg);
        } else if (cmd == "out") {
            VMState state = dbg.step_out();
            print_where(dbg);
            if (state != VMState::PAUSED)
                print_state(state, dbg);
        } else if (cmd == "run" || cmd == "r") {
            VMState state;
            if (args.size() > 1) {
                uint64_t n = std::stoull(args[1]);
                state = dbg.run_for(n);
            } else {
                state = dbg.run();
            }
            print_state(state, dbg);
            print_where(dbg);
        } else if (cmd == "where") {
            print_where(dbg);
        } else if (cmd == "vars") {
            auto vars = dbg.get_all_variables();
            if (vars.empty()) {
                std::cout << "  (no variables in scope)\n";
            } else {
                for (const auto& v : vars) {
                    print_variable(v);
                }
            }
        } else if (cmd == "var") {
            if (args.size() < 2) {
                std::cout << "Usage: var <name>\n";
                continue;
            }
            auto v = dbg.get_variable(args[1]);
            if (v) {
                print_variable(*v);
            } else {
                std::cout << "Variable not found: " << args[1] << "\n";
            }
        } else if (cmd == "eval") {
            if (args.size() < 2) {
                std::cout << "Usage: eval <expr>\n";
                continue;
            }
            // Rejoin all args after "eval" as the expression
            std::string expr;
            for (size_t i = 1; i < args.size(); ++i) {
                if (i > 1) expr += " ";
                expr += args[i];
            }
            auto eval_result = dbg.evaluate(expr);
            if (eval_result) {
                std::cout << "  = " << *eval_result << "\n";
            } else {
                std::cout << "Cannot evaluate: " << expr << "\n";
            }
        } else if (cmd == "inspect") {
            if (args.size() < 3) {
                std::cout << "Usage: inspect <address> <class_name>\n";
                continue;
            }
            try {
                Address addr = static_cast<Address>(std::stoul(args[1]));
                auto obj = dbg.inspect_object(addr, args[2]);
                std::cout << ObjectInspector::format_object(obj);
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "array") {
            if (args.size() < 3) {
                std::cout << "Usage: array <address> <length>\n";
                continue;
            }
            try {
                Address addr = static_cast<Address>(std::stoul(args[1]));
                size_t len = std::stoull(args[2]);
                auto arr = dbg.inspect_array(addr, len);
                std::cout << ObjectInspector::format_array(arr);
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "this") {
            try {
                auto obj = dbg.inspect_this();
                std::cout << ObjectInspector::format_object(obj);
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "calls") {
            auto frames = dbg.get_jack_call_stack();
            if (frames.empty()) {
                std::cout << "  (no call frames)\n";
            } else {
                for (size_t i = 0; i < frames.size(); ++i) {
                    const auto& f = frames[i];
                    std::cout << "  " << i << ": " << f.function_name;
                    if (!f.jack_file.empty())
                        std::cout << " (" << f.jack_file << ":" << f.jack_line << ")";
                    std::cout << "\n";
                }
            }
        } else if (cmd == "break" || cmd == "b") {
            if (args.size() < 3) {
                std::cout << "Usage: break <file> <line>\n";
                continue;
            }
            LineNumber ln = static_cast<LineNumber>(std::stoul(args[2]));
            bool ok = dbg.add_breakpoint(args[1], ln);
            if (ok) {
                std::cout << "Breakpoint set at " << args[1] << ":" << ln << "\n";
            } else {
                std::cout << "No mapping found for " << args[1] << ":" << ln << "\n";
            }
        } else if (cmd == "clear") {
            if (args.size() < 3) {
                std::cout << "Usage: clear <file> <line>\n";
                continue;
            }
            LineNumber ln = static_cast<LineNumber>(std::stoul(args[2]));
            dbg.remove_breakpoint(args[1], ln);
            std::cout << "Breakpoint cleared at " << args[1] << ":" << ln << "\n";
        } else if (cmd == "breaks") {
            auto bps = dbg.get_breakpoints();
            if (bps.empty()) {
                std::cout << "No breakpoints set.\n";
            } else {
                std::cout << "Breakpoints:\n";
                for (const auto& [file, ln] : bps) {
                    std::cout << "  " << file << ":" << ln << "\n";
                }
            }
        } else if (cmd == "stats") {
            print_stats(dbg);
        } else if (cmd == "reset") {
            dbg.reset();
            std::cout << "Debugger reset.\n";
        } else {
            std::cout << "Unknown command: " << cmd << ". Type 'help' for commands.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--help" || arg1 == "-h") {
        print_usage();
        return 0;
    }

    if (arg1 == "--run") {
        if (argc < 4) {
            std::cerr << "Error: --run requires a .vm file and source files\n";
            return 1;
        }
        std::string vm_path = argv[2];
        std::string smap_path;
        std::vector<std::string> jack_paths;
        uint64_t max_instr = 0;

        // Classify remaining args: .jack files, .smap file, or -n flag
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "-n" && i + 1 < argc) {
                max_instr = std::stoull(argv[++i]);
            } else if (ends_with(a, ".jack")) {
                jack_paths.push_back(a);
            } else if (ends_with(a, ".smap")) {
                smap_path = a;
            } else {
                // Assume it's a .jack or .smap file by context
                smap_path = a;
            }
        }

        return batch_mode(vm_path, smap_path, jack_paths, max_instr);
    }

    if (argc < 3) {
        std::cerr << "Error: requires a .vm file and source (.smap or .jack) files\n";
        print_usage();
        return 1;
    }

    // Classify arguments: first is .vm, rest are .jack or .smap
    std::string vm_path = arg1;
    std::string smap_path;
    std::vector<std::string> jack_paths;

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (ends_with(a, ".jack")) {
            jack_paths.push_back(a);
        } else if (ends_with(a, ".smap")) {
            smap_path = a;
        } else {
            smap_path = a;
        }
    }

    if (jack_paths.empty() && smap_path.empty()) {
        std::cerr << "Error: requires .smap or .jack files\n";
        print_usage();
        return 1;
    }

    interactive_mode(vm_path, smap_path, jack_paths);
    return 0;
}
