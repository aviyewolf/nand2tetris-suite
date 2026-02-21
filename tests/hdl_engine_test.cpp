// ==============================================================================
// HDL Engine Tests
// ==============================================================================

#include "hdl_engine.hpp"
#include "hdl_parser.hpp"
#include "hdl_chip.hpp"
#include "hdl_builtins.hpp"
#include "tst_runner.hpp"
#include "error.hpp"
#include <iostream>
#include <cassert>

using namespace n2t;

static int test_count = 0;
static int pass_count = 0;

static void check(bool condition, const std::string& name) {
    test_count++;
    if (condition) {
        pass_count++;
        std::cout << "PASS: " << name << std::endl;
    } else {
        std::cout << "FAIL: " << name << std::endl;
        assert(false);
    }
}

// ==============================================================================
// Parser Tests
// ==============================================================================

void test_parse_simple_chip() {
    std::cout << "\n--- Parser: Simple Chip ---\n";

    HDLParser parser;
    auto def = parser.parse_string(R"(
        CHIP And {
            IN a, b;
            OUT out;
            PARTS:
            Nand(a=a, b=b, out=nandOut);
            Nand(a=nandOut, b=nandOut, out=out);
        }
    )");

    check(def.name == "And", "Chip name is And");
    check(def.inputs.size() == 2, "Two inputs");
    check(def.inputs[0].name == "a", "First input is a");
    check(def.inputs[1].name == "b", "Second input is b");
    check(def.outputs.size() == 1, "One output");
    check(def.outputs[0].name == "out", "Output is out");
    check(def.parts.size() == 2, "Two parts");
    check(def.parts[0].chip_name == "Nand", "First part is Nand");
    check(def.parts[0].connections.size() == 3, "First part has 3 connections");
    check(!def.is_builtin, "Not builtin");
}

void test_parse_bus_ports() {
    std::cout << "\n--- Parser: Bus Ports ---\n";

    HDLParser parser;
    auto def = parser.parse_string(R"(
        CHIP Add16 {
            IN a[16], b[16];
            OUT out[16];
            BUILTIN Add16;
        }
    )", "Add16.hdl");

    check(def.inputs.size() == 2, "Two inputs");
    check(def.inputs[0].width == 16, "First input is 16-bit");
    check(def.inputs[1].width == 16, "Second input is 16-bit");
    check(def.outputs[0].width == 16, "Output is 16-bit");
    check(def.is_builtin, "Is builtin");
}

void test_parse_bit_range_subscripts() {
    std::cout << "\n--- Parser: Bit-range Subscripts ---\n";

    HDLParser parser;
    auto def = parser.parse_string(R"(
        CHIP Mux4Way16 {
            IN a[16], b[16], c[16], d[16], sel[2];
            OUT out[16];
            PARTS:
            Mux16(a=a, b=b, sel=sel[0], out=ab);
            Mux16(a=c, b=d, sel=sel[0], out=cd);
            Mux16(a=ab, b=cd, sel=sel[1], out=out);
        }
    )");

    check(def.parts.size() == 3, "Three parts");
    // Check that sel[0] was parsed correctly
    auto& conn = def.parts[0].connections[2]; // sel=sel[0]
    check(conn.external.name == "sel", "External pin is sel");
    check(conn.external.lo == 0, "Subscript lo is 0");
    check(conn.external.hi == 0, "Subscript hi is 0");
}

void test_parse_true_false() {
    std::cout << "\n--- Parser: true/false Constants ---\n";

    HDLParser parser;
    auto def = parser.parse_string(R"(
        CHIP AlwaysTrue {
            IN in;
            OUT out;
            PARTS:
            Or(a=in, b=true, out=out);
        }
    )");

    check(def.parts.size() == 1, "One part");
    check(def.parts[0].connections[1].external.name == "true",
          "Second connection external is true");
}

void test_parse_comments() {
    std::cout << "\n--- Parser: Comments ---\n";

    HDLParser parser;
    auto def = parser.parse_string(R"(
        // This is a line comment
        CHIP Not {
            IN in; /* block comment */
            OUT out;
            /* Multi-line
               comment */
            PARTS:
            Nand(a=in, b=in, out=out); // inline comment
        }
    )");

    check(def.name == "Not", "Chip name parsed through comments");
    check(def.parts.size() == 1, "One part");
}

void test_parse_builtin() {
    std::cout << "\n--- Parser: BUILTIN ---\n";

    HDLParser parser;
    auto def = parser.parse_string(R"(
        CHIP Nand {
            IN a, b;
            OUT out;
            BUILTIN Nand;
        }
    )");

    check(def.is_builtin, "Is builtin");
    check(def.parts.empty(), "No parts for builtin");
}

void test_parse_error_bad_syntax() {
    std::cout << "\n--- Parser: Error Cases ---\n";

    HDLParser parser;
    bool caught = false;
    try {
        parser.parse_string("CHIP { }");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "Parse error on missing chip name");

    caught = false;
    try {
        parser.parse_string("CHIP Foo { }");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "Parse error on missing IN keyword");
}

// ==============================================================================
// Builtin Chip Tests
// ==============================================================================

// Helper to create a builtin chip
static std::unique_ptr<HDLChip> make_builtin(const std::string& name) {
    const auto& reg = get_builtin_registry();
    auto it = reg.find(name);
    if (it == reg.end()) return nullptr;
    return std::make_unique<HDLChip>(it->second.def, it->second.eval_fn);
}

void test_builtin_nand() {
    std::cout << "\n--- Builtins: Nand ---\n";

    auto chip = make_builtin("Nand");
    check(chip != nullptr, "Nand chip created");

    // Truth table
    int expected[4] = {1, 1, 1, 0}; // Nand truth table
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            chip->set_pin("a", a);
            chip->set_pin("b", b);
            chip->eval();
            check(chip->get_pin("out") == expected[idx],
                  "Nand(" + std::to_string(a) + "," + std::to_string(b) + ")=" +
                  std::to_string(expected[idx]));
            idx++;
        }
    }
}

void test_builtin_not() {
    std::cout << "\n--- Builtins: Not ---\n";

    auto chip = make_builtin("Not");
    chip->set_pin("in", 0); chip->eval();
    check(chip->get_pin("out") == 1, "Not(0)=1");
    chip->set_pin("in", 1); chip->eval();
    check(chip->get_pin("out") == 0, "Not(1)=0");
}

void test_builtin_and() {
    std::cout << "\n--- Builtins: And ---\n";

    auto chip = make_builtin("And");
    int expected[4] = {0, 0, 0, 1};
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            chip->set_pin("a", a);
            chip->set_pin("b", b);
            chip->eval();
            check(chip->get_pin("out") == expected[idx],
                  "And(" + std::to_string(a) + "," + std::to_string(b) + ")=" +
                  std::to_string(expected[idx]));
            idx++;
        }
    }
}

void test_builtin_or() {
    std::cout << "\n--- Builtins: Or ---\n";

    auto chip = make_builtin("Or");
    int expected[4] = {0, 1, 1, 1};
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            chip->set_pin("a", a);
            chip->set_pin("b", b);
            chip->eval();
            check(chip->get_pin("out") == expected[idx],
                  "Or(" + std::to_string(a) + "," + std::to_string(b) + ")=" +
                  std::to_string(expected[idx]));
            idx++;
        }
    }
}

void test_builtin_xor() {
    std::cout << "\n--- Builtins: Xor ---\n";

    auto chip = make_builtin("Xor");
    int expected[4] = {0, 1, 1, 0};
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            chip->set_pin("a", a);
            chip->set_pin("b", b);
            chip->eval();
            check(chip->get_pin("out") == expected[idx],
                  "Xor(" + std::to_string(a) + "," + std::to_string(b) + ")=" +
                  std::to_string(expected[idx]));
            idx++;
        }
    }
}

void test_builtin_mux() {
    std::cout << "\n--- Builtins: Mux ---\n";

    auto chip = make_builtin("Mux");
    chip->set_pin("a", 0); chip->set_pin("b", 1); chip->set_pin("sel", 0);
    chip->eval();
    check(chip->get_pin("out") == 0, "Mux(0,1,sel=0)=0");

    chip->set_pin("sel", 1);
    chip->eval();
    check(chip->get_pin("out") == 1, "Mux(0,1,sel=1)=1");
}

void test_builtin_dmux() {
    std::cout << "\n--- Builtins: DMux ---\n";

    auto chip = make_builtin("DMux");
    chip->set_pin("in", 1); chip->set_pin("sel", 0);
    chip->eval();
    check(chip->get_pin("a") == 1 && chip->get_pin("b") == 0, "DMux(1,sel=0)={1,0}");

    chip->set_pin("sel", 1);
    chip->eval();
    check(chip->get_pin("a") == 0 && chip->get_pin("b") == 1, "DMux(1,sel=1)={0,1}");
}

void test_builtin_not16() {
    std::cout << "\n--- Builtins: Not16 ---\n";

    auto chip = make_builtin("Not16");
    chip->set_pin("in", 0x0000); chip->eval();
    check(chip->get_pin("out") == 0xFFFF, "Not16(0x0000)=0xFFFF");

    chip->set_pin("in", 0xFFFF); chip->eval();
    check(chip->get_pin("out") == 0x0000, "Not16(0xFFFF)=0x0000");

    chip->set_pin("in", 0xAAAA); chip->eval();
    check(chip->get_pin("out") == 0x5555, "Not16(0xAAAA)=0x5555");
}

void test_builtin_and16() {
    std::cout << "\n--- Builtins: And16 ---\n";

    auto chip = make_builtin("And16");
    chip->set_pin("a", 0xFF00); chip->set_pin("b", 0x0FF0); chip->eval();
    check(chip->get_pin("out") == 0x0F00, "And16(0xFF00,0x0FF0)=0x0F00");
}

void test_builtin_or16() {
    std::cout << "\n--- Builtins: Or16 ---\n";

    auto chip = make_builtin("Or16");
    chip->set_pin("a", 0xFF00); chip->set_pin("b", 0x0FF0); chip->eval();
    check(chip->get_pin("out") == 0xFFF0, "Or16(0xFF00,0x0FF0)=0xFFF0");
}

void test_builtin_mux16() {
    std::cout << "\n--- Builtins: Mux16 ---\n";

    auto chip = make_builtin("Mux16");
    chip->set_pin("a", 0x1234); chip->set_pin("b", 0x5678); chip->set_pin("sel", 0);
    chip->eval();
    check(chip->get_pin("out") == 0x1234, "Mux16 sel=0 selects a");

    chip->set_pin("sel", 1); chip->eval();
    check(chip->get_pin("out") == 0x5678, "Mux16 sel=1 selects b");
}

void test_builtin_or8way() {
    std::cout << "\n--- Builtins: Or8Way ---\n";

    auto chip = make_builtin("Or8Way");
    chip->set_pin("in", 0x00); chip->eval();
    check(chip->get_pin("out") == 0, "Or8Way(0x00)=0");

    chip->set_pin("in", 0x01); chip->eval();
    check(chip->get_pin("out") == 1, "Or8Way(0x01)=1");

    chip->set_pin("in", 0xFF); chip->eval();
    check(chip->get_pin("out") == 1, "Or8Way(0xFF)=1");
}

void test_builtin_mux4way16() {
    std::cout << "\n--- Builtins: Mux4Way16 ---\n";

    auto chip = make_builtin("Mux4Way16");
    chip->set_pin("a", 1); chip->set_pin("b", 2);
    chip->set_pin("c", 3); chip->set_pin("d", 4);

    chip->set_pin("sel", 0); chip->eval();
    check(chip->get_pin("out") == 1, "Mux4Way16 sel=00 selects a");

    chip->set_pin("sel", 1); chip->eval();
    check(chip->get_pin("out") == 2, "Mux4Way16 sel=01 selects b");

    chip->set_pin("sel", 2); chip->eval();
    check(chip->get_pin("out") == 3, "Mux4Way16 sel=10 selects c");

    chip->set_pin("sel", 3); chip->eval();
    check(chip->get_pin("out") == 4, "Mux4Way16 sel=11 selects d");
}

void test_builtin_mux8way16() {
    std::cout << "\n--- Builtins: Mux8Way16 ---\n";

    auto chip = make_builtin("Mux8Way16");
    const char* names[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    for (int i = 0; i < 8; i++) {
        chip->set_pin(names[i], i + 10);
    }
    for (int sel = 0; sel < 8; sel++) {
        chip->set_pin("sel", sel); chip->eval();
        check(chip->get_pin("out") == sel + 10,
              "Mux8Way16 sel=" + std::to_string(sel));
    }
}

void test_builtin_dmux4way() {
    std::cout << "\n--- Builtins: DMux4Way ---\n";

    auto chip = make_builtin("DMux4Way");
    chip->set_pin("in", 1);
    const char* outs[] = {"a", "b", "c", "d"};
    for (int sel = 0; sel < 4; sel++) {
        chip->set_pin("sel", sel); chip->eval();
        for (int o = 0; o < 4; o++) {
            check(chip->get_pin(outs[o]) == (o == sel ? 1 : 0),
                  "DMux4Way sel=" + std::to_string(sel) + " " + outs[o]);
        }
    }
}

void test_builtin_dmux8way() {
    std::cout << "\n--- Builtins: DMux8Way ---\n";

    auto chip = make_builtin("DMux8Way");
    chip->set_pin("in", 1);
    const char* outs[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    for (int sel = 0; sel < 8; sel++) {
        chip->set_pin("sel", sel); chip->eval();
        for (int o = 0; o < 8; o++) {
            check(chip->get_pin(outs[o]) == (o == sel ? 1 : 0),
                  "DMux8Way sel=" + std::to_string(sel) + " " + outs[o]);
        }
    }
}

void test_builtin_halfadder() {
    std::cout << "\n--- Builtins: HalfAdder ---\n";

    auto chip = make_builtin("HalfAdder");
    // 0+0=0,carry=0
    chip->set_pin("a", 0); chip->set_pin("b", 0); chip->eval();
    check(chip->get_pin("sum") == 0 && chip->get_pin("carry") == 0, "HA(0,0)={0,0}");

    // 0+1=1,carry=0
    chip->set_pin("a", 0); chip->set_pin("b", 1); chip->eval();
    check(chip->get_pin("sum") == 1 && chip->get_pin("carry") == 0, "HA(0,1)={1,0}");

    // 1+0=1,carry=0
    chip->set_pin("a", 1); chip->set_pin("b", 0); chip->eval();
    check(chip->get_pin("sum") == 1 && chip->get_pin("carry") == 0, "HA(1,0)={1,0}");

    // 1+1=0,carry=1
    chip->set_pin("a", 1); chip->set_pin("b", 1); chip->eval();
    check(chip->get_pin("sum") == 0 && chip->get_pin("carry") == 1, "HA(1,1)={0,1}");
}

void test_builtin_fulladder() {
    std::cout << "\n--- Builtins: FullAdder ---\n";

    auto chip = make_builtin("FullAdder");
    // 1+1+1=1,carry=1
    chip->set_pin("a", 1); chip->set_pin("b", 1); chip->set_pin("c", 1);
    chip->eval();
    check(chip->get_pin("sum") == 1 && chip->get_pin("carry") == 1,
          "FA(1,1,1)={1,1}");

    // 1+1+0=0,carry=1
    chip->set_pin("a", 1); chip->set_pin("b", 1); chip->set_pin("c", 0);
    chip->eval();
    check(chip->get_pin("sum") == 0 && chip->get_pin("carry") == 1,
          "FA(1,1,0)={0,1}");
}

void test_builtin_add16() {
    std::cout << "\n--- Builtins: Add16 ---\n";

    auto chip = make_builtin("Add16");
    chip->set_pin("a", 100); chip->set_pin("b", 200); chip->eval();
    check(chip->get_pin("out") == 300, "Add16(100,200)=300");

    chip->set_pin("a", 0xFFFF); chip->set_pin("b", 1); chip->eval();
    check(chip->get_pin("out") == 0, "Add16(0xFFFF,1)=0 (overflow)");
}

void test_builtin_inc16() {
    std::cout << "\n--- Builtins: Inc16 ---\n";

    auto chip = make_builtin("Inc16");
    chip->set_pin("in", 0); chip->eval();
    check(chip->get_pin("out") == 1, "Inc16(0)=1");

    chip->set_pin("in", 0xFFFF); chip->eval();
    check(chip->get_pin("out") == 0, "Inc16(0xFFFF)=0 (overflow)");
}

void test_builtin_alu() {
    std::cout << "\n--- Builtins: ALU ---\n";

    auto chip = make_builtin("ALU");

    // Zero: zx=1, nx=0, zy=1, ny=0, f=1, no=0 => 0
    chip->set_pin("x", 42); chip->set_pin("y", 99);
    chip->set_pin("zx", 1); chip->set_pin("nx", 0);
    chip->set_pin("zy", 1); chip->set_pin("ny", 0);
    chip->set_pin("f", 1); chip->set_pin("no", 0);
    chip->eval();
    check(chip->get_pin("out") == 0, "ALU: 0");
    check(chip->get_pin("zr") == 1, "ALU: zr=1 for zero");
    check(chip->get_pin("ng") == 0, "ALU: ng=0 for zero");

    // One: zx=1, nx=1, zy=1, ny=1, f=1, no=1 => 1
    chip->set_pin("zx", 1); chip->set_pin("nx", 1);
    chip->set_pin("zy", 1); chip->set_pin("ny", 1);
    chip->set_pin("f", 1); chip->set_pin("no", 1);
    chip->eval();
    check(chip->get_pin("out") == 1, "ALU: 1");

    // Minus one: zx=1, nx=1, zy=1, ny=0, f=1, no=0 => -1 (0xFFFF)
    chip->set_pin("zx", 1); chip->set_pin("nx", 1);
    chip->set_pin("zy", 1); chip->set_pin("ny", 0);
    chip->set_pin("f", 1); chip->set_pin("no", 0);
    chip->eval();
    check(chip->get_pin("out") == 0xFFFF, "ALU: -1");
    check(chip->get_pin("ng") == 1, "ALU: ng=1 for -1");

    // x: zx=0, nx=0, zy=1, ny=1, f=0, no=0 => x
    chip->set_pin("x", 12345); chip->set_pin("y", 0);
    chip->set_pin("zx", 0); chip->set_pin("nx", 0);
    chip->set_pin("zy", 1); chip->set_pin("ny", 1);
    chip->set_pin("f", 0); chip->set_pin("no", 0);
    chip->eval();
    check(chip->get_pin("out") == 12345, "ALU: x");

    // x+y: zx=0, nx=0, zy=0, ny=0, f=1, no=0 => x+y
    chip->set_pin("x", 100); chip->set_pin("y", 200);
    chip->set_pin("zx", 0); chip->set_pin("nx", 0);
    chip->set_pin("zy", 0); chip->set_pin("ny", 0);
    chip->set_pin("f", 1); chip->set_pin("no", 0);
    chip->eval();
    check(chip->get_pin("out") == 300, "ALU: x+y=300");

    // x&y: zx=0, nx=0, zy=0, ny=0, f=0, no=0 => x&y
    chip->set_pin("x", 0xFF00); chip->set_pin("y", 0x0FF0);
    chip->set_pin("zx", 0); chip->set_pin("nx", 0);
    chip->set_pin("zy", 0); chip->set_pin("ny", 0);
    chip->set_pin("f", 0); chip->set_pin("no", 0);
    chip->eval();
    check(chip->get_pin("out") == 0x0F00, "ALU: x&y");
}

// ==============================================================================
// Composite Chip Tests
// ==============================================================================

void test_composite_and_from_nand() {
    std::cout << "\n--- Composite: And from Nand ---\n";

    HDLEngine engine;
    engine.load_hdl_string(R"(
        CHIP And {
            IN a, b;
            OUT out;
            PARTS:
            Nand(a=a, b=b, out=nandOut);
            Not(in=nandOut, out=out);
        }
    )");

    check(engine.get_state() != HDLState::ERROR, "And chip loaded");

    // Test truth table
    int expected[4] = {0, 0, 0, 1};
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            engine.set_input("a", a);
            engine.set_input("b", b);
            engine.eval();
            check(engine.get_output("out") == expected[idx],
                  "Composite And(" + std::to_string(a) + "," +
                  std::to_string(b) + ")=" + std::to_string(expected[idx]));
            idx++;
        }
    }
}

void test_composite_or_from_not_nand() {
    std::cout << "\n--- Composite: Or from Not+Nand ---\n";

    HDLEngine engine;
    engine.load_hdl_string(R"(
        CHIP Or {
            IN a, b;
            OUT out;
            PARTS:
            Not(in=a, out=notA);
            Not(in=b, out=notB);
            Nand(a=notA, b=notB, out=out);
        }
    )");

    check(engine.get_state() != HDLState::ERROR, "Or chip loaded");

    int expected[4] = {0, 1, 1, 1};
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            engine.set_input("a", a);
            engine.set_input("b", b);
            engine.eval();
            check(engine.get_output("out") == expected[idx],
                  "Composite Or(" + std::to_string(a) + "," +
                  std::to_string(b) + ")=" + std::to_string(expected[idx]));
            idx++;
        }
    }
}

void test_composite_xor_multi_level() {
    std::cout << "\n--- Composite: Xor (multi-level) ---\n";

    HDLEngine engine;
    engine.load_hdl_string(R"(
        CHIP Xor {
            IN a, b;
            OUT out;
            PARTS:
            Not(in=a, out=notA);
            Not(in=b, out=notB);
            And(a=a, b=notB, out=aAndNotB);
            And(a=notA, b=b, out=notAAndB);
            Or(a=aAndNotB, b=notAAndB, out=out);
        }
    )");

    check(engine.get_state() != HDLState::ERROR, "Xor chip loaded");

    int expected[4] = {0, 1, 1, 0};
    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            engine.set_input("a", a);
            engine.set_input("b", b);
            engine.eval();
            check(engine.get_output("out") == expected[idx],
                  "Composite Xor(" + std::to_string(a) + "," +
                  std::to_string(b) + ")=" + std::to_string(expected[idx]));
            idx++;
        }
    }
}

// ==============================================================================
// Test Runner Tests
// ==============================================================================

void test_tst_runner_and() {
    std::cout << "\n--- TstRunner: And script ---\n";

    HDLEngine engine;
    std::string tst = R"(
        load And.hdl;
        output-list a%B1.1.1 b%B1.1.1 out%B1.1.1;

        set a 0, set b 0, eval, output;
        set a 0, set b 1, eval, output;
        set a 1, set b 0, eval, output;
        set a 1, set b 1, eval, output;
    )";

    std::string cmp =
        "| a | b |out|\n"
        "| 0 | 0 | 0 |\n"
        "| 0 | 1 | 0 |\n"
        "| 1 | 0 | 0 |\n"
        "| 1 | 1 | 1 |\n";

    auto state = engine.run_test_string(tst, cmp);
    check(!engine.has_comparison_error(), "And test passes comparison");
    if (engine.has_comparison_error()) {
        std::cout << "  Error: " << engine.get_error_message() << std::endl;
    }
    check(state == HDLState::HALTED, "Test halted successfully");
}

void test_tst_runner_not() {
    std::cout << "\n--- TstRunner: Not script ---\n";

    HDLEngine engine;
    std::string tst = R"(
        load Not.hdl;
        output-list in%B1.1.1 out%B1.1.1;

        set in 0, eval, output;
        set in 1, eval, output;
    )";

    std::string cmp =
        "|in |out|\n"
        "| 0 | 1 |\n"
        "| 1 | 0 |\n";

    auto state = engine.run_test_string(tst, cmp);
    check(!engine.has_comparison_error(), "Not test passes comparison");
    check(state == HDLState::HALTED, "Test halted successfully");
}

// ==============================================================================
// Output Formatting Tests
// ==============================================================================

void test_output_format_decimal() {
    std::cout << "\n--- Output Format: Decimal ---\n";

    HDLEngine engine;
    std::string tst = R"(
        load Add16.hdl;
        output-list a%D1.6.1 b%D1.6.1 out%D1.6.1;

        set a 100, set b 200, eval, output;
    )";

    engine.run_test_string(tst);
    std::string output = engine.get_output_table();
    check(output.find("100") != std::string::npos, "Output contains 100");
    check(output.find("200") != std::string::npos, "Output contains 200");
    check(output.find("300") != std::string::npos, "Output contains 300");
}

void test_output_format_binary() {
    std::cout << "\n--- Output Format: Binary ---\n";

    HDLEngine engine;
    std::string tst = R"(
        load And.hdl;
        output-list a%B1.1.1 b%B1.1.1 out%B1.1.1;

        set a 1, set b 1, eval, output;
    )";

    engine.run_test_string(tst);
    std::string output = engine.get_output_table();
    // Should have pipe-delimited binary values
    check(output.find("|") != std::string::npos, "Output has pipe delimiters");
}

// ==============================================================================
// Comparison Tests
// ==============================================================================

void test_comparison_pass() {
    std::cout << "\n--- Comparison: Pass ---\n";

    HDLEngine engine;
    std::string tst = R"(
        load Or.hdl;
        output-list a%B1.1.1 b%B1.1.1 out%B1.1.1;

        set a 0, set b 0, eval, output;
        set a 1, set b 1, eval, output;
    )";

    std::string cmp =
        "| a | b |out|\n"
        "| 0 | 0 | 0 |\n"
        "| 1 | 1 | 1 |\n";

    engine.run_test_string(tst, cmp);
    check(!engine.has_comparison_error(), "Comparison passes");
}

void test_comparison_fail() {
    std::cout << "\n--- Comparison: Fail ---\n";

    HDLEngine engine;
    std::string tst = R"(
        load Or.hdl;
        output-list a%B1.1.1 b%B1.1.1 out%B1.1.1;

        set a 0, set b 0, eval, output;
    )";

    // Wrong expected value
    std::string cmp =
        "| a | b |out|\n"
        "| 0 | 0 | 1 |\n";

    engine.run_test_string(tst, cmp);
    check(engine.has_comparison_error(), "Comparison detects mismatch");
    check(engine.get_error_message().find("Comparison failure") != std::string::npos,
          "Error message mentions comparison failure");
}

// ==============================================================================
// Error Handling Tests
// ==============================================================================

void test_error_unknown_chip() {
    std::cout << "\n--- Error: Unknown Chip ---\n";

    HDLEngine engine;
    engine.load_hdl_string(R"(
        CHIP Bad {
            IN a;
            OUT out;
            PARTS:
            NonExistentChip(in=a, out=out);
        }
    )");

    check(engine.get_state() == HDLState::ERROR, "Error state for unknown chip");
}

void test_error_unknown_pin() {
    std::cout << "\n--- Error: Unknown Pin ---\n";

    HDLEngine engine;
    engine.load_hdl_string(R"(
        CHIP Not {
            IN in;
            OUT out;
            PARTS:
            Nand(a=in, b=in, out=out);
        }
    )");

    engine.set_input("nonexistent", 1);
    check(engine.get_state() == HDLState::ERROR, "Error on unknown pin");
}

void test_error_bad_hdl() {
    std::cout << "\n--- Error: Bad HDL ---\n";

    HDLEngine engine;
    engine.load_hdl_string("this is not valid HDL");
    check(engine.get_state() == HDLState::ERROR, "Error on bad HDL");
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "==================================================\n";
    std::cout << "HDL Engine Tests\n";
    std::cout << "==================================================\n";

    // Parser tests
    test_parse_simple_chip();
    test_parse_bus_ports();
    test_parse_bit_range_subscripts();
    test_parse_true_false();
    test_parse_comments();
    test_parse_builtin();
    test_parse_error_bad_syntax();

    // Builtin tests
    test_builtin_nand();
    test_builtin_not();
    test_builtin_and();
    test_builtin_or();
    test_builtin_xor();
    test_builtin_mux();
    test_builtin_dmux();
    test_builtin_not16();
    test_builtin_and16();
    test_builtin_or16();
    test_builtin_mux16();
    test_builtin_or8way();
    test_builtin_mux4way16();
    test_builtin_mux8way16();
    test_builtin_dmux4way();
    test_builtin_dmux8way();
    test_builtin_halfadder();
    test_builtin_fulladder();
    test_builtin_add16();
    test_builtin_inc16();
    test_builtin_alu();

    // Composite chip tests
    test_composite_and_from_nand();
    test_composite_or_from_not_nand();
    test_composite_xor_multi_level();

    // Test runner tests
    test_tst_runner_and();
    test_tst_runner_not();

    // Output format tests
    test_output_format_decimal();
    test_output_format_binary();

    // Comparison tests
    test_comparison_pass();
    test_comparison_fail();

    // Error handling tests
    test_error_unknown_chip();
    test_error_unknown_pin();
    test_error_bad_hdl();

    std::cout << "\n==================================================\n";
    std::cout << "Results: " << pass_count << "/" << test_count << " passed\n";
    std::cout << "==================================================\n";

    return (pass_count == test_count) ? 0 : 1;
}
