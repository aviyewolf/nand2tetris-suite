// ==============================================================================
// cpu_sim — Hack CPU Simulator CLI
// ==============================================================================
// Batch:       cpu_sim --run Prog.hack [-n 1000]
// Interactive: cpu_sim Prog.hack
// ==============================================================================

#include "cpu.hpp"
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
              << "  cpu_sim --run Prog.hack [-n <max_instructions>]   Run in batch mode\n"
              << "  cpu_sim Prog.hack                                  Interactive REPL\n"
              << "  cpu_sim --help                                     Show this help\n";
}

static void print_regs(const CPUEngine& cpu) {
    std::cout << "  A  = " << cpu.get_a()
              << " (0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_a() << std::dec << ")\n"
              << "  D  = " << cpu.get_d()
              << " (0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_d() << std::dec << ")\n"
              << "  PC = " << cpu.get_pc() << "\n";
}

static void print_state(CPUState state, const CPUEngine& cpu) {
    switch (state) {
        case CPUState::READY:   std::cout << "[READY]\n"; break;
        case CPUState::RUNNING: std::cout << "[RUNNING]\n"; break;
        case CPUState::PAUSED:
            std::cout << "[PAUSED]";
            if (cpu.get_pause_reason() == CPUPauseReason::BREAKPOINT)
                std::cout << " breakpoint at PC=" << cpu.get_pc();
            std::cout << "\n";
            break;
        case CPUState::HALTED:  std::cout << "[HALTED] PC past end of ROM\n"; break;
        case CPUState::ERROR:   std::cout << "[ERROR] " << cpu.get_error_message() << "\n"; break;
    }
}

static void print_stats(const CPUEngine& cpu) {
    const auto& s = cpu.get_stats();
    std::cout << "  Instructions executed: " << s.instructions_executed << "\n"
              << "  A-instructions:        " << s.a_instruction_count << "\n"
              << "  C-instructions:        " << s.c_instruction_count << "\n"
              << "  Jumps taken:           " << s.jump_count << "\n"
              << "  Memory reads:          " << s.memory_reads << "\n"
              << "  Memory writes:         " << s.memory_writes << "\n";
}

static std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) args.push_back(token);
    return args;
}

static int batch_mode(const std::string& file, uint64_t max_instr) {
    CPUEngine cpu;
    try {
        cpu.load_file(file);
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    CPUState state = (max_instr > 0) ? cpu.run_for(max_instr) : cpu.run();

    print_state(state, cpu);
    print_regs(cpu);
    std::cout << "\n";
    print_stats(cpu);

    return (state == CPUState::ERROR) ? 1 : 0;
}

static void interactive_mode(const std::string& file) {
    CPUEngine cpu;
    try {
        cpu.load_file(file);
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "Hack CPU Simulator — " << file << "\n"
              << "ROM: " << cpu.rom_size() << " instructions loaded\n"
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
        } else if (cmd == "help" || cmd == "h") {
            std::cout << "Commands:\n"
                      << "  step [N], s [N]      Step N instructions (default 1)\n"
                      << "  run, r               Run until halt/breakpoint\n"
                      << "  run N                Run for N instructions\n"
                      << "  regs                 Show A, D, PC registers\n"
                      << "  ram <addr> [count]   Show RAM contents\n"
                      << "  rom <addr> [count]   Show ROM with disassembly\n"
                      << "  break <addr>, b      Set breakpoint\n"
                      << "  clear <addr>         Clear breakpoint\n"
                      << "  breaks               List breakpoints\n"
                      << "  dasm [addr] [count]  Disassemble instructions\n"
                      << "  stats                Show execution statistics\n"
                      << "  reset                Reset CPU\n"
                      << "  quit, q              Exit\n";
        } else if (cmd == "step" || cmd == "s") {
            unsigned n = 1;
            if (args.size() > 1) n = static_cast<unsigned>(std::stoul(args[1]));
            CPUState state = CPUState::READY;
            for (unsigned i = 0; i < n; ++i) {
                state = cpu.step();
                if (state != CPUState::PAUSED) break;
            }
            // Show current instruction location
            std::cout << "PC=" << cpu.get_pc();
            if (cpu.get_pc() < cpu.rom_size()) {
                std::cout << "  " << cpu.disassemble(cpu.get_pc());
            }
            std::cout << "\n";
            if (state == CPUState::HALTED || state == CPUState::ERROR)
                print_state(state, cpu);
        } else if (cmd == "run" || cmd == "r") {
            CPUState state;
            if (args.size() > 1) {
                uint64_t n = std::stoull(args[1]);
                state = cpu.run_for(n);
            } else {
                state = cpu.run();
            }
            print_state(state, cpu);
            print_regs(cpu);
        } else if (cmd == "regs") {
            print_regs(cpu);
        } else if (cmd == "ram") {
            if (args.size() < 2) {
                std::cout << "Usage: ram <addr> [count]\n";
                continue;
            }
            Address addr = static_cast<Address>(std::stoul(args[1]));
            unsigned count = (args.size() > 2) ? static_cast<unsigned>(std::stoul(args[2])) : 1;
            for (unsigned i = 0; i < count; ++i) {
                Address a = static_cast<Address>(addr + i);
                Word val = cpu.read_ram(a);
                std::cout << "  RAM[" << a << "] = " << static_cast<int16_t>(val)
                          << " (0x" << std::hex << std::setfill('0') << std::setw(4) << val << std::dec << ")\n";
            }
        } else if (cmd == "rom") {
            if (args.size() < 2) {
                std::cout << "Usage: rom <addr> [count]\n";
                continue;
            }
            Address addr = static_cast<Address>(std::stoul(args[1]));
            unsigned count = (args.size() > 2) ? static_cast<unsigned>(std::stoul(args[2])) : 1;
            for (unsigned i = 0; i < count; ++i) {
                Address a = static_cast<Address>(addr + i);
                if (a < cpu.rom_size()) {
                    std::cout << "  " << std::setw(5) << a << ": " << cpu.disassemble(a) << "\n";
                }
            }
        } else if (cmd == "break" || cmd == "b") {
            if (args.size() < 2) {
                std::cout << "Usage: break <addr>\n";
                continue;
            }
            Address addr = static_cast<Address>(std::stoul(args[1]));
            cpu.add_breakpoint(addr);
            std::cout << "Breakpoint set at ROM[" << addr << "]\n";
        } else if (cmd == "clear") {
            if (args.size() < 2) {
                std::cout << "Usage: clear <addr>\n";
                continue;
            }
            Address addr = static_cast<Address>(std::stoul(args[1]));
            cpu.remove_breakpoint(addr);
            std::cout << "Breakpoint cleared at ROM[" << addr << "]\n";
        } else if (cmd == "breaks") {
            auto bps = cpu.get_breakpoints();
            if (bps.empty()) {
                std::cout << "No breakpoints set.\n";
            } else {
                std::cout << "Breakpoints:\n";
                for (auto a : bps)
                    std::cout << "  ROM[" << a << "]\n";
            }
        } else if (cmd == "dasm") {
            Address addr = cpu.get_pc();
            unsigned count = 10;
            if (args.size() > 1) addr = static_cast<Address>(std::stoul(args[1]));
            if (args.size() > 2) count = static_cast<unsigned>(std::stoul(args[2]));
            Address end = static_cast<Address>(std::min(static_cast<size_t>(addr + count), cpu.rom_size()));
            auto lines = cpu.disassemble_range(addr, end);
            for (unsigned i = 0; i < lines.size(); ++i) {
                Address a = static_cast<Address>(addr + i);
                std::cout << (cpu.has_breakpoint(a) ? "* " : "  ")
                          << std::setw(5) << a << ": " << lines[i];
                if (a == cpu.get_pc()) std::cout << "  <-- PC";
                std::cout << "\n";
            }
        } else if (cmd == "stats") {
            print_stats(cpu);
        } else if (cmd == "reset") {
            cpu.reset();
            std::cout << "CPU reset.\n";
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
            std::cerr << "Error: --run requires a .hack file\n";
            return 1;
        }
        uint64_t max_instr = 0;
        if (argc >= 5 && std::string(argv[3]) == "-n") {
            max_instr = std::stoull(argv[4]);
        }
        return batch_mode(argv[2], max_instr);
    }

    // Interactive mode
    interactive_mode(arg1);
    return 0;
}
