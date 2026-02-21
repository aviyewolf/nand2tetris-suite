// ==============================================================================
// HDL Built-in Chips Implementation
// ==============================================================================

#include "hdl_builtins.hpp"
#include "hdl_chip.hpp"
#include <cstdint>

namespace n2t {

// Helper: make a chip def with given inputs/outputs (all single-bit unless specified)
static HDLChipDef make_def(const std::string& name,
                           std::vector<HDLPort> inputs,
                           std::vector<HDLPort> outputs) {
    HDLChipDef def;
    def.name = name;
    def.inputs = std::move(inputs);
    def.outputs = std::move(outputs);
    def.is_builtin = true;
    return def;
}

// Shorthand for single-bit port
static HDLPort pin1(const std::string& name) { return {name, 1}; }
// Shorthand for 16-bit port
static HDLPort pin16(const std::string& name) { return {name, 16}; }

// Helper to get 16-bit value as signed
static int16_t to_signed16(int64_t v) {
    auto u = static_cast<uint16_t>(v & 0xFFFF);
    return static_cast<int16_t>(u);
}

// ==============================================================================
// Primitive Gate
// ==============================================================================

void eval_Nand(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a & b) ? 0 : 1);
}

// ==============================================================================
// Basic Gates
// ==============================================================================

void eval_Not(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    chip.set_pin("out", in ? 0 : 1);
}

void eval_And(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a & b) & 1);
}

void eval_Or(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a | b) & 1);
}

void eval_Xor(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a ^ b) & 1);
}

void eval_Mux(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    int64_t sel = chip.get_pin("sel");
    chip.set_pin("out", sel ? b : a);
}

void eval_DMux(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    int64_t sel = chip.get_pin("sel");
    chip.set_pin("a", sel ? 0 : in);
    chip.set_pin("b", sel ? in : 0);
}

// ==============================================================================
// 16-bit Variants
// ==============================================================================

void eval_Not16(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    chip.set_pin("out", (~in) & 0xFFFF);
}

void eval_And16(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a & b) & 0xFFFF);
}

void eval_Or16(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a | b) & 0xFFFF);
}

void eval_Mux16(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    int64_t sel = chip.get_pin("sel");
    chip.set_pin("out", sel ? (b & 0xFFFF) : (a & 0xFFFF));
}

// ==============================================================================
// Multi-way
// ==============================================================================

void eval_Or8Way(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    chip.set_pin("out", (in & 0xFF) ? 1 : 0);
}

void eval_Mux4Way16(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    int64_t c = chip.get_pin("c");
    int64_t d = chip.get_pin("d");
    int64_t sel = chip.get_pin("sel");
    int64_t result;
    switch (sel & 3) {
        case 0: result = a; break;
        case 1: result = b; break;
        case 2: result = c; break;
        case 3: result = d; break;
        default: result = 0; break;
    }
    chip.set_pin("out", result & 0xFFFF);
}

void eval_Mux8Way16(HDLChip& chip) {
    int64_t inputs[8];
    const char* names[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    for (int i = 0; i < 8; i++) {
        inputs[i] = chip.get_pin(names[i]);
    }
    int64_t sel = chip.get_pin("sel");
    chip.set_pin("out", inputs[sel & 7] & 0xFFFF);
}

void eval_DMux4Way(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    int64_t sel = chip.get_pin("sel");
    chip.set_pin("a", (sel & 3) == 0 ? in : 0);
    chip.set_pin("b", (sel & 3) == 1 ? in : 0);
    chip.set_pin("c", (sel & 3) == 2 ? in : 0);
    chip.set_pin("d", (sel & 3) == 3 ? in : 0);
}

void eval_DMux8Way(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    int64_t sel = chip.get_pin("sel");
    const char* names[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    for (int i = 0; i < 8; i++) {
        chip.set_pin(names[i], (sel & 7) == i ? in : 0);
    }
}

// ==============================================================================
// Arithmetic
// ==============================================================================

void eval_HalfAdder(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("sum", (a ^ b) & 1);
    chip.set_pin("carry", (a & b) & 1);
}

void eval_FullAdder(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    int64_t c = chip.get_pin("c");
    int64_t s = a + b + c;
    chip.set_pin("sum", s & 1);
    chip.set_pin("carry", (s >> 1) & 1);
}

void eval_Add16(HDLChip& chip) {
    int64_t a = chip.get_pin("a");
    int64_t b = chip.get_pin("b");
    chip.set_pin("out", (a + b) & 0xFFFF);
}

void eval_Inc16(HDLChip& chip) {
    int64_t in = chip.get_pin("in");
    chip.set_pin("out", (in + 1) & 0xFFFF);
}

// ==============================================================================
// ALU
// ==============================================================================

void eval_ALU(HDLChip& chip) {
    int64_t x = chip.get_pin("x");
    int64_t y = chip.get_pin("y");
    int64_t zx = chip.get_pin("zx");
    int64_t nx = chip.get_pin("nx");
    int64_t zy = chip.get_pin("zy");
    int64_t ny = chip.get_pin("ny");
    int64_t f = chip.get_pin("f");
    int64_t no = chip.get_pin("no");

    // Zero x
    if (zx) x = 0;
    // Negate x
    if (nx) x = (~x) & 0xFFFF;
    // Zero y
    if (zy) y = 0;
    // Negate y
    if (ny) y = (~y) & 0xFFFF;

    // Compute
    int64_t out;
    if (f) {
        out = (x + y) & 0xFFFF;
    } else {
        out = (x & y) & 0xFFFF;
    }

    // Negate output
    if (no) out = (~out) & 0xFFFF;

    chip.set_pin("out", out);

    // Status flags
    int16_t signed_out = to_signed16(out);
    chip.set_pin("zr", (out == 0) ? 1 : 0);
    chip.set_pin("ng", (signed_out < 0) ? 1 : 0);
}

// ==============================================================================
// Registry
// ==============================================================================

const std::unordered_map<std::string, BuiltinInfo>& get_builtin_registry() {
    static std::unordered_map<std::string, BuiltinInfo> registry;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;

        auto add = [&](const std::string& name,
                       std::vector<HDLPort> inputs,
                       std::vector<HDLPort> outputs,
                       std::function<void(HDLChip&)> fn) {
            registry[name] = {make_def(name, std::move(inputs), std::move(outputs)),
                              std::move(fn)};
        };

        // Primitive
        add("Nand", {pin1("a"), pin1("b")}, {pin1("out")}, eval_Nand);

        // Basic gates
        add("Not", {pin1("in")}, {pin1("out")}, eval_Not);
        add("And", {pin1("a"), pin1("b")}, {pin1("out")}, eval_And);
        add("Or", {pin1("a"), pin1("b")}, {pin1("out")}, eval_Or);
        add("Xor", {pin1("a"), pin1("b")}, {pin1("out")}, eval_Xor);
        add("Mux", {pin1("a"), pin1("b"), pin1("sel")}, {pin1("out")}, eval_Mux);
        add("DMux", {pin1("in"), pin1("sel")}, {pin1("a"), pin1("b")}, eval_DMux);

        // 16-bit
        add("Not16", {pin16("in")}, {pin16("out")}, eval_Not16);
        add("And16", {pin16("a"), pin16("b")}, {pin16("out")}, eval_And16);
        add("Or16", {pin16("a"), pin16("b")}, {pin16("out")}, eval_Or16);
        add("Mux16", {pin16("a"), pin16("b"), pin1("sel")}, {pin16("out")}, eval_Mux16);

        // Multi-way
        add("Or8Way", {{("in"), 8}}, {pin1("out")}, eval_Or8Way);
        add("Mux4Way16", {pin16("a"), pin16("b"), pin16("c"), pin16("d"),
                          {("sel"), 2}}, {pin16("out")}, eval_Mux4Way16);
        add("Mux8Way16", {pin16("a"), pin16("b"), pin16("c"), pin16("d"),
                          pin16("e"), pin16("f"), pin16("g"), pin16("h"),
                          {("sel"), 3}}, {pin16("out")}, eval_Mux8Way16);
        add("DMux4Way", {pin1("in"), {("sel"), 2}},
            {pin1("a"), pin1("b"), pin1("c"), pin1("d")}, eval_DMux4Way);
        add("DMux8Way", {pin1("in"), {("sel"), 3}},
            {pin1("a"), pin1("b"), pin1("c"), pin1("d"),
             pin1("e"), pin1("f"), pin1("g"), pin1("h")}, eval_DMux8Way);

        // Arithmetic
        add("HalfAdder", {pin1("a"), pin1("b")},
            {pin1("sum"), pin1("carry")}, eval_HalfAdder);
        add("FullAdder", {pin1("a"), pin1("b"), pin1("c")},
            {pin1("sum"), pin1("carry")}, eval_FullAdder);
        add("Add16", {pin16("a"), pin16("b")}, {pin16("out")}, eval_Add16);
        add("Inc16", {pin16("in")}, {pin16("out")}, eval_Inc16);

        // ALU
        add("ALU", {pin16("x"), pin16("y"),
                     pin1("zx"), pin1("nx"), pin1("zy"), pin1("ny"),
                     pin1("f"), pin1("no")},
            {pin16("out"), pin1("zr"), pin1("ng")}, eval_ALU);
    }

    return registry;
}

}  // namespace n2t
