// ==============================================================================
// VM Engine Basic Tests
// ==============================================================================
// Simple test program to verify the VM engine works correctly.
// Uses assertions rather than a test framework for simplicity.
// ==============================================================================

#include "vm_engine.hpp"
#include <iostream>
#include <cassert>

using namespace n2t;

// Helper to run a VM program from source and check the stack
void test_program(const std::string& name, const std::string& source,
                  const std::vector<Word>& expected_stack) {
    VMEngine vm;
    vm.load_string(source, "test");

    // For simple programs without Sys.init, start from command 0
    vm.set_entry_point("");
    vm.reset();

    VMState result = vm.run_for(10000);

    auto stack = vm.get_stack();

    bool pass = (stack == expected_stack);
    std::cout << (pass ? "PASS" : "FAIL") << ": " << name;
    if (!pass) {
        std::cout << "\n  Expected stack: [";
        for (size_t i = 0; i < expected_stack.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << static_cast<int16_t>(expected_stack[i]);
        }
        std::cout << "]\n  Actual stack:   [";
        for (size_t i = 0; i < stack.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << static_cast<int16_t>(stack[i]);
        }
        std::cout << "]";
        if (result == VMState::ERROR) {
            std::cout << "\n  Error: " << vm.get_error_message();
        }
    }
    std::cout << "\n";

    assert(pass);
}

// Helper for programs with function calls
void test_function_program(const std::string& name, const std::string& source,
                           Word expected_return_value) {
    VMEngine vm;
    vm.load_string(source, "test");

    VMState result = vm.run_for(100000);

    // After halting, the return value should be at the top of the stack
    auto stack = vm.get_stack();
    bool pass = (result == VMState::HALTED) && !stack.empty() && stack.back() == expected_return_value;
    std::cout << (pass ? "PASS" : "FAIL") << ": " << name;
    if (!pass) {
        std::cout << "\n  Expected return: " << static_cast<int16_t>(expected_return_value);
        std::cout << "\n  Stack: [";
        for (size_t i = 0; i < stack.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << static_cast<int16_t>(stack[i]);
        }
        std::cout << "]";
        std::cout << "\n  State: " << static_cast<int>(result);
        if (result == VMState::ERROR) {
            std::cout << "\n  Error: " << vm.get_error_message();
            std::cout << "\n  Error at cmd: " << vm.get_error_location();
        }
        std::cout << "\n  PC: " << vm.get_pc();
        std::cout << "\n  Stats: instr=" << vm.get_stats().instructions_executed;
    }
    std::cout << std::endl;

    assert(pass);
}

int main() {
    std::cout << "=== VM Engine Tests ===\n\n";

    // ---- Arithmetic Tests ----
    std::cout << "--- Arithmetic ---\n";

    test_program("push constant",
        "push constant 7\n",
        {7});

    test_program("add",
        "push constant 7\n"
        "push constant 8\n"
        "add\n",
        {15});

    test_program("sub",
        "push constant 10\n"
        "push constant 3\n"
        "sub\n",
        {7});

    test_program("neg",
        "push constant 5\n"
        "neg\n",
        {static_cast<Word>(-5)});

    test_program("eq true",
        "push constant 5\n"
        "push constant 5\n"
        "eq\n",
        {0xFFFF});

    test_program("eq false",
        "push constant 5\n"
        "push constant 6\n"
        "eq\n",
        {0});

    test_program("gt true",
        "push constant 10\n"
        "push constant 5\n"
        "gt\n",
        {0xFFFF});

    test_program("gt false",
        "push constant 5\n"
        "push constant 10\n"
        "gt\n",
        {0});

    test_program("lt true",
        "push constant 3\n"
        "push constant 7\n"
        "lt\n",
        {0xFFFF});

    test_program("and",
        "push constant 5\n"   // 0101
        "push constant 3\n"   // 0011
        "and\n",
        {1});                  // 0001

    test_program("or",
        "push constant 5\n"   // 0101
        "push constant 3\n"   // 0011
        "or\n",
        {7});                  // 0111

    test_program("not",
        "push constant 0\n"
        "not\n",
        {0xFFFF});

    // ---- Multiple operations ----
    std::cout << "\n--- Compound ---\n";

    test_program("(2 + 3) * (4 - 1) via stack",
        "push constant 2\n"
        "push constant 3\n"
        "add\n"
        "push constant 4\n"
        "push constant 1\n"
        "sub\n",
        {5, 3});  // stack has 5 and 3

    // ---- Function call tests ----
    std::cout << "\n--- Function Calls ---\n";

    test_function_program("simple function call",
        "function Sys.init 0\n"
        "push constant 3\n"
        "push constant 4\n"
        "call Math.add 2\n"
        "return\n"
        "function Math.add 0\n"
        "push argument 0\n"
        "push argument 1\n"
        "add\n"
        "return\n",
        7);

    test_function_program("nested function calls",
        "function Sys.init 0\n"
        "push constant 10\n"
        "call Math.double 1\n"
        "return\n"
        "function Math.double 0\n"
        "push argument 0\n"
        "push argument 0\n"
        "add\n"
        "return\n",
        20);

    // ---- Control flow tests ----
    std::cout << "\n--- Control Flow ---\n";

    test_function_program("if-goto (true branch)",
        "function Sys.init 0\n"
        "push constant 1\n"
        "if-goto TRUE_BRANCH\n"
        "push constant 99\n"
        "return\n"
        "label TRUE_BRANCH\n"
        "push constant 42\n"
        "return\n",
        42);

    test_function_program("if-goto (false branch)",
        "function Sys.init 0\n"
        "push constant 0\n"
        "if-goto TRUE_BRANCH\n"
        "push constant 99\n"
        "return\n"
        "label TRUE_BRANCH\n"
        "push constant 42\n"
        "return\n",
        99);

    // ---- Local variables ----
    std::cout << "\n--- Local Variables ---\n";

    test_function_program("local variables",
        "function Sys.init 2\n"
        "push constant 10\n"
        "pop local 0\n"
        "push constant 20\n"
        "pop local 1\n"
        "push local 0\n"
        "push local 1\n"
        "add\n"
        "return\n",
        30);

    // ---- Statistics ----
    std::cout << "\n--- Statistics ---\n";
    {
        VMEngine vm;
        vm.load_string(
            "function Sys.init 0\n"
            "push constant 1\n"
            "push constant 2\n"
            "add\n"
            "return\n",
            "test");
        vm.run();
        auto stats = vm.get_stats();
        bool pass = stats.instructions_executed == 5  // function + push + push + add + return
                 && stats.push_count == 2
                 && stats.arithmetic_count == 1
                 && stats.return_count == 1;
        std::cout << (pass ? "PASS" : "FAIL") << ": execution statistics"
                  << " (instr=" << stats.instructions_executed
                  << ", push=" << stats.push_count
                  << ", arith=" << stats.arithmetic_count
                  << ", ret=" << stats.return_count << ")\n";
        assert(pass);
    }

    // ---- Debugging ----
    std::cout << "\n--- Debugging ---\n";
    {
        // Use a program with Sys.init; bootstrap frame adds 5 values to stack
        VMEngine vm;
        vm.load_string(
            "function Sys.init 0\n"   // cmd 0
            "push constant 10\n"      // cmd 1
            "push constant 20\n"      // cmd 2
            "push constant 30\n"      // cmd 3
            "return\n",              // cmd 4
            "test");

        // Step through: first step executes "function Sys.init 0"
        vm.step();  // function Sys.init 0 (no-op at runtime, bootstrap frame already set up)
        vm.step();  // push constant 10
        auto stack = vm.get_stack();
        // Stack has 5 bootstrap frame values + the pushed 10
        bool pass1 = stack.back() == 10;
        std::cout << (pass1 ? "PASS" : "FAIL") << ": step execution\n";
        assert(pass1);

        vm.step();  // push constant 20
        vm.step();  // push constant 30
        stack = vm.get_stack();
        bool pass2 = stack.back() == 30;
        std::cout << (pass2 ? "PASS" : "FAIL") << ": continued stepping\n";
        assert(pass2);
    }

    // ---- Breakpoints ----
    {
        VMEngine vm;
        vm.load_string(
            "function Sys.init 0\n"   // cmd 0
            "push constant 10\n"      // cmd 1
            "push constant 20\n"      // cmd 2
            "push constant 30\n"      // cmd 3
            "return\n",              // cmd 4
            "test");

        vm.add_breakpoint(3);  // Break at "push constant 30"
        VMState state = vm.run();
        bool pass = state == VMState::PAUSED
                 && vm.get_pause_reason() == PauseReason::BREAKPOINT
                 && vm.get_pc() == 3;
        std::cout << (pass ? "PASS" : "FAIL") << ": breakpoint hit\n";
        assert(pass);

        // Continue after breakpoint
        vm.clear_breakpoints();
        state = vm.run();
        bool pass2 = state == VMState::HALTED;
        std::cout << (pass2 ? "PASS" : "FAIL") << ": continue after breakpoint\n";
        assert(pass2);
    }

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
