// ==============================================================================
// HDL Built-in Chips Implementation
// ==============================================================================

#include "hdl_builtins.hpp"
#include "hdl_chip.hpp"
#include <cstdint>
#include <vector>
#include <any>

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
// Sequential Chips
// ==============================================================================

// --- DFF ---
// out[t+1] = in[t]
void eval_DFF(HDLChip& chip) {
    // eval just reflects current DFF state to out
    chip.set_pin("out", chip.get_dff_state());
}

void tick_DFF(HDLChip& chip) {
    // Rising edge: sample input
    chip.set_dff_next(chip.get_pin("in"));
}

void tock_DFF(HDLChip& chip) {
    // Falling edge: commit sampled input to state and update out
    chip.set_dff_state(chip.get_dff_next());
    chip.set_pin("out", chip.get_dff_state());
}

// --- RAM state helper ---
struct RAMState {
    std::vector<int64_t> memory;
    int64_t pending_value = 0;
    int64_t pending_address = 0;
    bool pending_write = false;

    explicit RAMState(size_t size) : memory(size, 0) {}
};

static RAMState& get_ram_state(HDLChip& chip, size_t size) {
    if (!chip.chip_state_.has_value()) {
        chip.chip_state_ = RAMState(size);
    }
    return std::any_cast<RAMState&>(chip.chip_state_);
}

// --- Bit (1-bit register) ---
// if load[t] then out[t+1] = in[t] else out[t+1] = out[t]
struct BitState {
    int64_t value = 0;
    int64_t pending = 0;
    bool pending_load = false;
};

static BitState& get_bit_state(HDLChip& chip) {
    if (!chip.chip_state_.has_value()) {
        chip.chip_state_ = BitState{};
    }
    return std::any_cast<BitState&>(chip.chip_state_);
}

void eval_Bit(HDLChip& chip) {
    auto& st = get_bit_state(chip);
    chip.set_pin("out", st.value);
}

void tick_Bit(HDLChip& chip) {
    auto& st = get_bit_state(chip);
    st.pending = chip.get_pin("in");
    st.pending_load = (chip.get_pin("load") != 0);
}

void tock_Bit(HDLChip& chip) {
    auto& st = get_bit_state(chip);
    if (st.pending_load) {
        st.value = st.pending & 1;
    }
    chip.set_pin("out", st.value);
}

// --- Register (16-bit) ---
struct RegisterState {
    int64_t value = 0;
    int64_t pending = 0;
    bool pending_load = false;
};

static RegisterState& get_register_state(HDLChip& chip) {
    if (!chip.chip_state_.has_value()) {
        chip.chip_state_ = RegisterState{};
    }
    return std::any_cast<RegisterState&>(chip.chip_state_);
}

void eval_Register(HDLChip& chip) {
    auto& st = get_register_state(chip);
    chip.set_pin("out", st.value);
}

void tick_Register(HDLChip& chip) {
    auto& st = get_register_state(chip);
    st.pending = chip.get_pin("in") & 0xFFFF;
    st.pending_load = (chip.get_pin("load") != 0);
}

void tock_Register(HDLChip& chip) {
    auto& st = get_register_state(chip);
    if (st.pending_load) {
        st.value = st.pending;
    }
    chip.set_pin("out", st.value);
}

// --- Generic RAM eval/tick/tock ---
template<size_t SIZE, int ADDR_BITS>
void eval_RAM(HDLChip& chip) {
    auto& st = get_ram_state(chip, SIZE);
    int64_t address = chip.get_pin("address") & ((1 << ADDR_BITS) - 1);
    chip.set_pin("out", st.memory[static_cast<size_t>(address)]);
}

template<size_t SIZE, int ADDR_BITS>
void tick_RAM(HDLChip& chip) {
    auto& st = get_ram_state(chip, SIZE);
    st.pending_address = chip.get_pin("address") & ((1 << ADDR_BITS) - 1);
    st.pending_value = chip.get_pin("in") & 0xFFFF;
    st.pending_write = (chip.get_pin("load") != 0);
}

template<size_t SIZE, int ADDR_BITS>
void tock_RAM(HDLChip& chip) {
    auto& st = get_ram_state(chip, SIZE);
    if (st.pending_write) {
        st.memory[static_cast<size_t>(st.pending_address)] = st.pending_value;
    }
    int64_t address = chip.get_pin("address") & ((1 << ADDR_BITS) - 1);
    chip.set_pin("out", st.memory[static_cast<size_t>(address)]);
}

// RAM8
void eval_RAM8(HDLChip& chip) { eval_RAM<8, 3>(chip); }
void tick_RAM8(HDLChip& chip) { tick_RAM<8, 3>(chip); }
void tock_RAM8(HDLChip& chip) { tock_RAM<8, 3>(chip); }

// RAM64
void eval_RAM64(HDLChip& chip) { eval_RAM<64, 6>(chip); }
void tick_RAM64(HDLChip& chip) { tick_RAM<64, 6>(chip); }
void tock_RAM64(HDLChip& chip) { tock_RAM<64, 6>(chip); }

// RAM512
void eval_RAM512(HDLChip& chip) { eval_RAM<512, 9>(chip); }
void tick_RAM512(HDLChip& chip) { tick_RAM<512, 9>(chip); }
void tock_RAM512(HDLChip& chip) { tock_RAM<512, 9>(chip); }

// RAM4K
void eval_RAM4K(HDLChip& chip) { eval_RAM<4096, 12>(chip); }
void tick_RAM4K(HDLChip& chip) { tick_RAM<4096, 12>(chip); }
void tock_RAM4K(HDLChip& chip) { tock_RAM<4096, 12>(chip); }

// RAM16K
void eval_RAM16K(HDLChip& chip) { eval_RAM<16384, 14>(chip); }
void tick_RAM16K(HDLChip& chip) { tick_RAM<16384, 14>(chip); }
void tock_RAM16K(HDLChip& chip) { tock_RAM<16384, 14>(chip); }

// --- PC (Program Counter) ---
// Priority: reset > load > inc > hold
// out[t+1] = 0        if reset[t]
// out[t+1] = in[t]    if load[t]
// out[t+1] = out[t]+1 if inc[t]
// out[t+1] = out[t]   otherwise
struct PCState {
    int64_t value = 0;
    int64_t pending = 0;
};

static PCState& get_pc_state(HDLChip& chip) {
    if (!chip.chip_state_.has_value()) {
        chip.chip_state_ = PCState{};
    }
    return std::any_cast<PCState&>(chip.chip_state_);
}

void eval_PC(HDLChip& chip) {
    auto& st = get_pc_state(chip);
    chip.set_pin("out", st.value);
}

void tick_PC(HDLChip& chip) {
    auto& st = get_pc_state(chip);
    int64_t reset = chip.get_pin("reset");
    int64_t load = chip.get_pin("load");
    int64_t inc = chip.get_pin("inc");
    int64_t in = chip.get_pin("in") & 0xFFFF;

    if (reset) {
        st.pending = 0;
    } else if (load) {
        st.pending = in;
    } else if (inc) {
        st.pending = (st.value + 1) & 0xFFFF;
    } else {
        st.pending = st.value;
    }
}

void tock_PC(HDLChip& chip) {
    auto& st = get_pc_state(chip);
    st.value = st.pending;
    chip.set_pin("out", st.value);
}

// ==============================================================================
// Registry
// ==============================================================================

const std::unordered_map<std::string, BuiltinInfo>& get_builtin_registry() {
    static std::unordered_map<std::string, BuiltinInfo> registry;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;

        // Helper for combinational chips (no tick/tock)
        auto add = [&](const std::string& name,
                       std::vector<HDLPort> inputs,
                       std::vector<HDLPort> outputs,
                       std::function<void(HDLChip&)> fn) {
            registry[name] = {make_def(name, std::move(inputs), std::move(outputs)),
                              std::move(fn), nullptr, nullptr};
        };

        // Helper for sequential chips (with tick/tock)
        auto add_seq = [&](const std::string& name,
                           std::vector<HDLPort> inputs,
                           std::vector<HDLPort> outputs,
                           std::function<void(HDLChip&)> eval,
                           std::function<void(HDLChip&)> tick,
                           std::function<void(HDLChip&)> tock) {
            registry[name] = {make_def(name, std::move(inputs), std::move(outputs)),
                              std::move(eval), std::move(tick), std::move(tock)};
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

        // =============== Sequential chips ===============

        // DFF primitive
        add_seq("DFF", {pin1("in")}, {pin1("out")},
                eval_DFF, tick_DFF, tock_DFF);

        // Bit (1-bit register)
        add_seq("Bit", {pin1("in"), pin1("load")}, {pin1("out")},
                eval_Bit, tick_Bit, tock_Bit);

        // Register (16-bit)
        add_seq("Register", {pin16("in"), pin1("load")}, {pin16("out")},
                eval_Register, tick_Register, tock_Register);

        // RAM chips
        add_seq("RAM8", {pin16("in"), pin1("load"), {("address"), 3}}, {pin16("out")},
                eval_RAM8, tick_RAM8, tock_RAM8);
        add_seq("RAM64", {pin16("in"), pin1("load"), {("address"), 6}}, {pin16("out")},
                eval_RAM64, tick_RAM64, tock_RAM64);
        add_seq("RAM512", {pin16("in"), pin1("load"), {("address"), 9}}, {pin16("out")},
                eval_RAM512, tick_RAM512, tock_RAM512);
        add_seq("RAM4K", {pin16("in"), pin1("load"), {("address"), 12}}, {pin16("out")},
                eval_RAM4K, tick_RAM4K, tock_RAM4K);
        add_seq("RAM16K", {pin16("in"), pin1("load"), {("address"), 14}}, {pin16("out")},
                eval_RAM16K, tick_RAM16K, tock_RAM16K);

        // PC (Program Counter)
        add_seq("PC", {pin16("in"), pin1("load"), pin1("inc"), pin1("reset")}, {pin16("out")},
                eval_PC, tick_PC, tock_PC);
    }

    return registry;
}

}  // namespace n2t
