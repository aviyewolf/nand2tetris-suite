// ==============================================================================
// hdl_sim — HDL Simulator CLI
// ==============================================================================
// Batch:       hdl_sim --test Chip.tst [--compare Chip.cmp]
// Interactive: hdl_sim Chip.hdl
// ==============================================================================

#include "hdl_engine.hpp"
#include "error.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace n2t;

static void print_usage() {
    std::cout << "Usage:\n"
              << "  hdl_sim --test Chip.tst [--compare Chip.cmp]   Run test script\n"
              << "  hdl_sim Chip.hdl                                Interactive REPL\n"
              << "  hdl_sim --help                                  Show this help\n";
}

static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::string dir_of(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

static std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) args.push_back(token);
    return args;
}

static int batch_mode(const std::string& tst_path, const std::string& cmp_path) {
    HDLEngine engine;

    // Add test file directory to search path
    engine.add_search_path(dir_of(tst_path));

    try {
        std::string tst_source = read_file(tst_path);
        std::string cmp_source;
        if (!cmp_path.empty()) {
            cmp_source = read_file(cmp_path);
        }

        HDLState state = engine.run_test_string(tst_source, cmp_source, tst_path);

        // Print output table
        const auto& output = engine.get_output_table();
        if (!output.empty()) {
            std::cout << output;
            if (output.back() != '\n') std::cout << "\n";
        }

        if (state == HDLState::ERROR) {
            std::cerr << "FAIL: " << engine.get_error_message() << "\n";
            return 1;
        }

        if (engine.has_comparison_error()) {
            std::cerr << "FAIL: Output does not match expected comparison file.\n";
            return 1;
        }

        const auto& stats = engine.get_stats();
        std::cout << "PASS (" << stats.eval_count << " evaluations, "
                  << stats.output_rows << " output rows)\n";
        return 0;

    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

static void interactive_mode(const std::string& hdl_path) {
    HDLEngine engine;

    // Add HDL file directory to search path
    engine.add_search_path(dir_of(hdl_path));

    try {
        engine.load_hdl_file(hdl_path);
    } catch (const N2TError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "HDL Simulator — " << hdl_path << "\n"
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
                      << "  set <pin> <value>   Set input pin\n"
                      << "  eval                Evaluate combinational logic\n"
                      << "  tick                Clock rising edge\n"
                      << "  tock                Clock falling edge\n"
                      << "  get <pin>           Read output pin\n"
                      << "  pins                List all pins with values\n"
                      << "  reset               Reset chip\n"
                      << "  quit, q             Exit\n";
        } else if (cmd == "set") {
            if (args.size() < 3) {
                std::cout << "Usage: set <pin> <value>\n";
                continue;
            }
            try {
                int64_t value = std::stoll(args[2]);
                engine.set_input(args[1], value);
                std::cout << args[1] << " = " << value << "\n";
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::cout << "Invalid value: " << args[2] << "\n";
            }
        } else if (cmd == "eval") {
            try {
                engine.eval();
                std::cout << "Evaluated.\n";
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "tick") {
            try {
                engine.tick();
                std::cout << "Tick.\n";
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "tock") {
            try {
                engine.tock();
                std::cout << "Tock.\n";
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "get") {
            if (args.size() < 2) {
                std::cout << "Usage: get <pin>\n";
                continue;
            }
            try {
                int64_t val = engine.get_output(args[1]);
                std::cout << args[1] << " = " << val << "\n";
            } catch (const N2TError& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        } else if (cmd == "pins") {
            // Use the engine's state to query — we need to work through
            // get_output for each known pin. Since HDLEngine doesn't expose
            // a pin list directly, we try the loaded chip's definition.
            // The engine manages the chip internally, so we rely on
            // set_input/get_output which throw on unknown pins.
            std::cout << "(Use 'set' and 'get' with specific pin names.\n"
                      << " Pin names are defined in the chip's HDL file.)\n";
        } else if (cmd == "reset") {
            engine.reset();
            std::cout << "Chip reset.\n";
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

    if (arg1 == "--test") {
        if (argc < 3) {
            std::cerr << "Error: --test requires a .tst file\n";
            return 1;
        }
        std::string cmp_path;
        if (argc >= 5 && std::string(argv[3]) == "--compare") {
            cmp_path = argv[4];
        }
        return batch_mode(argv[2], cmp_path);
    }

    interactive_mode(arg1);
    return 0;
}
