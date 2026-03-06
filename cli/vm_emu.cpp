// ==============================================================================
// vm_emu — VM Emulator CLI
// ==============================================================================
// Batch:       vm_emu --run Prog.vm [-n 10000]
// Interactive: vm_emu Prog.vm  |  vm_emu dir/
// ==============================================================================

#include "vm_engine.hpp"
#include "vm_command.hpp"
#include "error.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <iomanip>

using namespace n2t;

static void print_usage() {
    std::cout << "Usage:\n"
              << "  vm_emu --run <file.vm|dir> [-n <max>]   Run in batch mode\n"
              << "  vm_emu <file.vm|dir>                     Interactive REPL\n"
              << "  vm_emu --help                            Show this help\n";
}

static void print_state(VMState state, const VMEngine& vm) {
    switch (state) {
        case VMState::READY:   std::cout << "[READY]\n"; break;
        case VMState::RUNNING: std::cout << "[RUNNING]\n"; break;
        case VMState::PAUSED:
            std::cout << "[PAUSED]";
            switch (vm.get_pause_reason()) {
                case PauseReason::BREAKPOINT:     std::cout << " breakpoint"; break;
                case PauseReason::FUNCTION_ENTRY:  std::cout << " function entry"; break;
                case PauseReason::FUNCTION_EXIT:   std::cout << " function exit"; break;
                default: break;
            }
            std::cout << " at cmd " << vm.get_pc() << "\n";
            break;
        case VMState::HALTED:  std::cout << "[HALTED]\n"; break;
        case VMState::ERROR:   std::cout << "[ERROR] " << vm.get_error_message() << "\n"; break;
    }
}

static void print_current_cmd(const VMEngine& vm) {
    auto* cmd = vm.get_current_command();
    if (cmd) {
        std::cout << "  " << vm.get_pc() << ": " << command_to_string(*cmd);
        auto func = vm.get_current_function();
        if (!func.empty()) std::cout << "  [" << func << "]";
        std::cout << "\n";
    }
}

static void print_stack(const VMEngine& vm) {
    auto stack = vm.get_stack();
    std::cout << "Stack (SP=" << vm.get_sp() << ", " << stack.size() << " entries):\n";
    if (stack.empty()) {
        std::cout << "  (empty)\n";
        return;
    }
    // Show top 20 entries, most recent first
    size_t start = (stack.size() > 20) ? stack.size() - 20 : 0;
    if (start > 0) std::cout << "  ... (" << start << " more entries)\n";
    for (size_t i = start; i < stack.size(); ++i) {
        Word val = stack[i];
        std::cout << "  [" << (256 + i) << "] " << static_cast<int16_t>(val)
                  << " (0x" << std::hex << std::setfill('0') << std::setw(4) << val << std::dec << ")";
        if (i == stack.size() - 1) std::cout << "  <-- top";
        std::cout << "\n";
    }
}

static void print_stats(const VMEngine& vm) {
    const auto& s = vm.get_stats();
    std::cout << "  Instructions executed: " << s.instructions_executed << "\n"
              << "  Push count:            " << s.push_count << "\n"
              << "  Pop count:             " << s.pop_count << "\n"
              << "  Arithmetic count:      " << s.arithmetic_count << "\n"
              << "  Call count:            " << s.call_count << "\n"
              << "  Return count:          " << s.return_count << "\n";
}

static std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) args.push_back(token);
    return args;
}

static bool is_directory(const std::string& path) {
    return !path.empty() && (path.back() == '/' || path.back() == '\\');
}

static void load_program(VMEngine& vm, const std::string& path) {
    if (is_directory(path)) {
        vm.load_directory(path);
    } else {
        vm.load_file(path);
    }
}

static int batch_mode(const std::string& path, uint64_t max_instr) {
    VMEngine vm;
    try {
        load_program(vm, path);
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    VMState state = (max_instr > 0) ? vm.run_for(max_instr) : vm.run();

    print_state(state, vm);
    print_current_cmd(vm);
    std::cout << "\n";
    print_stack(vm);
    std::cout << "\n";
    print_stats(vm);

    return (state == VMState::ERROR) ? 1 : 0;
}

static const char* segment_name(const std::string& name) {
    // Return nullptr if not a valid segment name
    static const char* valid[] = {
        "local", "argument", "this", "that", "temp", "static", "pointer", "constant"
    };
    for (auto v : valid)
        if (name == v) return v;
    return nullptr;
}

static SegmentType parse_segment(const std::string& name) {
    if (name == "local")    return SegmentType::LOCAL;
    if (name == "argument") return SegmentType::ARGUMENT;
    if (name == "this")     return SegmentType::THIS;
    if (name == "that")     return SegmentType::THAT;
    if (name == "temp")     return SegmentType::TEMP;
    if (name == "static")   return SegmentType::STATIC;
    if (name == "pointer")  return SegmentType::POINTER;
    if (name == "constant") return SegmentType::CONSTANT;
    return SegmentType::LOCAL; // fallback
}

static void interactive_mode(const std::string& path) {
    VMEngine vm;
    try {
        load_program(vm, path);
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "VM Emulator — " << path << "\n"
              << vm.get_command_count() << " commands loaded\n"
              << "Type 'help' for commands.\n\n";

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        auto args = split_args(line);
        if (args.empty()) continue;

        const auto& cmd = args[0];

        if (cmd == "quit" || cmd == "q") {
            break;
        } else if (cmd == "help") {
            std::cout << "Commands:\n"
                      << "  step [N], s [N]       Step N VM commands (default 1)\n"
                      << "  over, n               Step over function call\n"
                      << "  out                   Step out of function\n"
                      << "  run, r                Run until halt/breakpoint\n"
                      << "  run N                 Run for N instructions\n"
                      << "  stack                 Show stack contents\n"
                      << "  seg <name> [index]    Show segment value\n"
                      << "  ram <addr> [count]    Show RAM\n"
                      << "  cmd [index]           Show current or specific VM command\n"
                      << "  calls                 Show call stack\n"
                      << "  break <idx>, b <idx>  Set breakpoint at command index\n"
                      << "  fbreak <func>         Break at function entry\n"
                      << "  clear <idx>           Clear breakpoint\n"
                      << "  breaks                List breakpoints\n"
                      << "  stats                 Execution statistics\n"
                      << "  reset                 Reset VM\n"
                      << "  quit, q               Exit\n";
        } else if (cmd == "step" || cmd == "s") {
            unsigned n = 1;
            if (args.size() > 1) n = static_cast<unsigned>(std::stoul(args[1]));
            VMState state = VMState::READY;
            for (unsigned i = 0; i < n; ++i) {
                state = vm.step();
                if (state != VMState::PAUSED) break;
            }
            print_current_cmd(vm);
            if (state == VMState::HALTED || state == VMState::ERROR)
                print_state(state, vm);
        } else if (cmd == "over" || cmd == "n") {
            VMState state = vm.step_over();
            print_current_cmd(vm);
            if (state != VMState::PAUSED)
                print_state(state, vm);
        } else if (cmd == "out") {
            VMState state = vm.step_out();
            print_current_cmd(vm);
            if (state != VMState::PAUSED)
                print_state(state, vm);
        } else if (cmd == "run" || cmd == "r") {
            VMState state;
            if (args.size() > 1) {
                uint64_t n = std::stoull(args[1]);
                state = vm.run_for(n);
            } else {
                state = vm.run();
            }
            print_state(state, vm);
            print_current_cmd(vm);
        } else if (cmd == "stack") {
            print_stack(vm);
        } else if (cmd == "seg") {
            if (args.size() < 2) {
                std::cout << "Usage: seg <name> [index]\n"
                          << "Segments: local, argument, this, that, temp, static, pointer\n";
                continue;
            }
            if (!segment_name(args[1])) {
                std::cout << "Unknown segment: " << args[1] << "\n";
                continue;
            }
            SegmentType seg = parse_segment(args[1]);
            if (args.size() > 2) {
                uint16_t idx = static_cast<uint16_t>(std::stoul(args[2]));
                try {
                    Word val = vm.get_segment(seg, idx);
                    std::cout << "  " << args[1] << "[" << idx << "] = "
                              << static_cast<int16_t>(val) << "\n";
                } catch (const N2TError& e) {
                    std::cout << "Error: " << e.what() << "\n";
                }
            } else {
                // Show first several entries
                unsigned count = (seg == SegmentType::TEMP) ? 8 :
                                 (seg == SegmentType::POINTER) ? 2 : 8;
                for (uint16_t i = 0; i < count; ++i) {
                    try {
                        Word val = vm.get_segment(seg, i);
                        std::cout << "  " << args[1] << "[" << i << "] = "
                                  << static_cast<int16_t>(val) << "\n";
                    } catch (...) {
                        break;
                    }
                }
            }
        } else if (cmd == "ram") {
            if (args.size() < 2) {
                std::cout << "Usage: ram <addr> [count]\n";
                continue;
            }
            Address addr = static_cast<Address>(std::stoul(args[1]));
            unsigned count = (args.size() > 2) ? static_cast<unsigned>(std::stoul(args[2])) : 1;
            for (unsigned i = 0; i < count; ++i) {
                Address a = static_cast<Address>(addr + i);
                Word val = vm.read_ram(a);
                std::cout << "  RAM[" << a << "] = " << static_cast<int16_t>(val)
                          << " (0x" << std::hex << std::setfill('0') << std::setw(4) << val << std::dec << ")\n";
            }
        } else if (cmd == "cmd") {
            if (args.size() > 1) {
                size_t idx = std::stoull(args[1]);
                auto* c = vm.get_command(idx);
                if (c) {
                    std::cout << "  " << idx << ": " << command_to_string(*c) << "\n";
                } else {
                    std::cout << "No command at index " << idx << "\n";
                }
            } else {
                print_current_cmd(vm);
            }
        } else if (cmd == "calls") {
            const auto& frames = vm.get_call_stack();
            if (frames.empty()) {
                std::cout << "  (no call frames)\n";
            } else {
                for (size_t i = frames.size(); i > 0; --i) {
                    const auto& f = frames[i - 1];
                    std::cout << "  " << (frames.size() - i) << ": " << f.function_name
                              << " (args=" << f.num_args << ", locals=" << f.num_locals << ")\n";
                }
            }
        } else if (cmd == "break" || cmd == "b") {
            if (args.size() < 2) {
                std::cout << "Usage: break <command_index>\n";
                continue;
            }
            size_t idx = std::stoull(args[1]);
            vm.add_breakpoint(idx);
            std::cout << "Breakpoint set at command " << idx << "\n";
        } else if (cmd == "fbreak") {
            if (args.size() < 2) {
                std::cout << "Usage: fbreak <function_name>\n";
                continue;
            }
            try {
                vm.add_function_breakpoint(args[1]);
                std::cout << "Breakpoint set at " << args[1] << "\n";
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "clear") {
            if (args.size() < 2) {
                std::cout << "Usage: clear <command_index>\n";
                continue;
            }
            size_t idx = std::stoull(args[1]);
            vm.remove_breakpoint(idx);
            std::cout << "Breakpoint cleared at command " << idx << "\n";
        } else if (cmd == "breaks") {
            auto bps = vm.get_breakpoints();
            if (bps.empty()) {
                std::cout << "No breakpoints set.\n";
            } else {
                std::cout << "Breakpoints:\n";
                for (auto idx : bps) {
                    std::cout << "  cmd " << idx;
                    auto* c = vm.get_command(idx);
                    if (c) std::cout << ": " << command_to_string(*c);
                    std::cout << "\n";
                }
            }
        } else if (cmd == "stats") {
            print_stats(vm);
        } else if (cmd == "reset") {
            vm.reset();
            std::cout << "VM reset.\n";
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
        if (argc < 3) {
            std::cerr << "Error: --run requires a .vm file or directory\n";
            return 1;
        }
        uint64_t max_instr = 0;
        if (argc >= 5 && std::string(argv[3]) == "-n") {
            max_instr = std::stoull(argv[4]);
        }
        return batch_mode(argv[2], max_instr);
    }

    interactive_mode(arg1);
    return 0;
}
