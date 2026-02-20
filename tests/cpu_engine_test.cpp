// ==============================================================================
// CPU Engine Tests
// ==============================================================================

#include "cpu.hpp"
#include "instruction.hpp"
#include "memory.hpp"
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
// Instruction Decoder Tests
// ==============================================================================

void test_decode_a_instruction() {
    std::cout << "--- Instruction Decoder ---\n";

    // @5 = 0000000000000101
    auto d = decode_instruction(0b0000000000000101);
    check(d.type == InstructionType::A_INSTRUCTION, "A-instruction @5 type");
    check(d.value == 5, "A-instruction @5 value");

    // @32767 (max A value) = 0111111111111111
    d = decode_instruction(0b0111111111111111);
    check(d.value == 32767, "A-instruction @32767 (max)");

    // @0 = 0000000000000000
    d = decode_instruction(0);
    check(d.type == InstructionType::A_INSTRUCTION && d.value == 0, "A-instruction @0");
}

void test_decode_c_instruction() {
    // D=A => 1110110000010000
    auto d = decode_instruction(0b1110110000010000);
    check(d.type == InstructionType::C_INSTRUCTION, "C-instruction D=A type");
    check(d.comp == Computation::A, "C-instruction D=A comp");
    check(d.dest.d && !d.dest.a && !d.dest.m, "C-instruction D=A dest");
    check(d.jump == JumpCondition::NO_JUMP, "C-instruction D=A jump");
    check(!d.reads_memory, "C-instruction D=A reads_memory");

    // M=D+M => 1111000010001000
    d = decode_instruction(0b1111000010001000);
    check(d.comp == Computation::D_PLUS_M, "C-instruction M=D+M comp");
    check(d.dest.m && !d.dest.a && !d.dest.d, "C-instruction M=D+M dest");
    check(d.reads_memory, "C-instruction M=D+M reads_memory");

    // D;JGT => 1110001100000001
    d = decode_instruction(0b1110001100000001);
    check(d.comp == Computation::D, "C-instruction D;JGT comp");
    check(!d.dest.a && !d.dest.d && !d.dest.m, "C-instruction D;JGT dest (none)");
    check(d.jump == JumpCondition::JGT, "C-instruction D;JGT jump");

    // 0;JMP => 1110101010000111
    d = decode_instruction(0b1110101010000111);
    check(d.comp == Computation::ZERO, "C-instruction 0;JMP comp");
    check(d.jump == JumpCondition::JMP, "C-instruction 0;JMP jump");

    // AMD=D+1 => 1110011111111000
    d = decode_instruction(0b1110011111111000);
    check(d.comp == Computation::D_PLUS_1, "C-instruction AMD=D+1 comp");
    check(d.dest.a && d.dest.d && d.dest.m, "C-instruction AMD=D+1 dest (all)");
}

void test_decode_checked() {
    // Valid instruction should not throw
    auto d = decode_instruction_checked(0b1110110000010000);
    check(d.comp == Computation::A, "checked decode valid instruction");

    // Invalid comp code should throw
    bool threw = false;
    try {
        // 111 0100100 010 000 — invalid comp bits 0100100
        decode_instruction_checked(0b1110100100010000);
    } catch (const ParseError&) {
        threw = true;
    }
    check(threw, "checked decode rejects invalid comp");
}

void test_disassembly() {
    check(instruction_to_string(static_cast<Word>(0b0000000000000101)) == "@5",
          "disassemble @5");
    check(instruction_to_string(static_cast<Word>(0b1110110000010000)) == "D=A",
          "disassemble D=A");
    check(instruction_to_string(static_cast<Word>(0b1110001100000001)) == "D;JGT",
          "disassemble D;JGT");
    check(instruction_to_string(static_cast<Word>(0b1110101010000111)) == "0;JMP",
          "disassemble 0;JMP");
    check(instruction_to_string(static_cast<Word>(0b1111000010001000)) == "M=D+M",
          "disassemble M=D+M");
    check(instruction_to_string(static_cast<Word>(0b1110011111111000)) == "ADM=D+1",
          "disassemble ADM=D+1");
}

// ==============================================================================
// Memory Tests
// ==============================================================================

void test_memory() {
    std::cout << "\n--- Memory ---\n";

    CPUMemory mem;

    // ROM loading from string
    mem.load_rom_string(
        "0000000000000101\n"   // @5
        "1110110000010000\n"); // D=A
    check(mem.rom_size() == 2, "ROM load: 2 instructions");
    check(mem.read_rom(0) == 0b0000000000000101, "ROM read instruction 0");
    check(mem.read_rom(1) == 0b1110110000010000, "ROM read instruction 1");

    // RAM read/write
    mem.write_ram(100, 42);
    check(mem.read_ram(100) == 42, "RAM write/read");

    // RAM bounds checking
    bool threw = false;
    try { mem.read_ram(40000); } catch (const RuntimeError&) { threw = true; }
    check(threw, "RAM read out of bounds throws");

    threw = false;
    try { mem.write_ram(40000, 1); } catch (const RuntimeError&) { threw = true; }
    check(threw, "RAM write out of bounds throws");

    // Screen dirty tracking
    mem.clear_screen_dirty();
    check(!mem.screen_dirty(), "screen clean after clear");
    mem.write_ram(16384, 0xFFFF);  // Write to screen memory
    check(mem.screen_dirty(), "screen dirty after write to screen address");

    // Pixel access
    mem.reset();
    mem.set_pixel(0, 0, true);
    check(mem.get_pixel(0, 0) == true, "pixel set/get (0,0)");
    check(mem.get_pixel(1, 0) == false, "pixel not set (1,0)");

    // Invalid .hack format
    threw = false;
    try { mem.load_rom_string("hello world\n"); } catch (const ParseError&) { threw = true; }
    check(threw, "invalid .hack format throws ParseError");

    threw = false;
    try { mem.load_rom_string("00001111000011x0\n"); } catch (const ParseError&) { threw = true; }
    check(threw, "invalid character in .hack throws ParseError");

    // Reset clears everything
    mem.write_ram(50, 999);
    mem.reset();
    check(mem.read_ram(50) == 0, "reset clears RAM");
    check(mem.rom_size() == 0, "reset clears ROM size");
}

// ==============================================================================
// CPU Engine Tests
// ==============================================================================

void test_cpu_set_d() {
    std::cout << "\n--- CPU Execution ---\n";

    // @5 / D=A
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000101\n"   // @5
        "1110110000010000\n"); // D=A
    cpu.run();
    check(cpu.get_d() == 5, "set D=5 via @5/D=A");
    check(cpu.get_a() == 5, "A=5 after @5");
    check(cpu.get_state() == CPUState::HALTED, "halts after 2 instructions");
}

void test_cpu_add() {
    // @2 / D=A / @3 / D=D+A => D should be 5
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000010\n"   // @2
        "1110110000010000\n"   // D=A
        "0000000000000011\n"   // @3
        "1110000010010000\n"); // D=D+A
    cpu.run();
    check(cpu.get_d() == 5, "D=2+3=5");
}

void test_cpu_write_ram() {
    // @10 / D=A / @100 / M=D => RAM[100]=10
    CPUEngine cpu;
    cpu.load_string(
        "0000000000001010\n"   // @10
        "1110110000010000\n"   // D=A
        "0000000001100100\n"   // @100
        "1110001100001000\n"); // M=D
    cpu.run();
    check(cpu.read_ram(100) == 10, "write RAM[100]=10");
}

void test_cpu_jump_taken() {
    // @5 / D=A / @10 / D;JGT
    // D=5 > 0 so should jump to ROM[10]
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000101\n"   // @5
        "1110110000010000\n"   // D=A
        "0000000000001010\n"   // @10
        "1110001100000001\n"); // D;JGT
    cpu.run();
    check(cpu.get_pc() == 10, "jump taken: PC=10 when D>0");
}

void test_cpu_jump_not_taken() {
    // @0 / D=A / @10 / D;JGT / @99 / D=A
    // D=0, no jump, continues to @99/D=A
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000000\n"   // @0
        "1110110000010000\n"   // D=A
        "0000000000001010\n"   // @10
        "1110001100000001\n"   // D;JGT
        "0000000001100011\n"   // @99
        "1110110000010000\n"); // D=A
    cpu.run();
    check(cpu.get_d() == 99, "jump not taken: D=99 when D==0");
}

void test_cpu_decrement_loop() {
    // Load 3 into RAM[0], decrement in a loop until 0
    //
    // @3         // 0: A=3
    // D=A        // 1: D=3
    // @0         // 2: A=0
    // M=D        // 3: RAM[0]=3
    // (LOOP)
    // @0         // 4: A=0
    // D=M        // 5: D=RAM[0]
    // @END       // 6: A=10
    // D;JEQ      // 7: if D==0 jump to END
    // @0         // 8: A=0
    // M=D-1      // 9: RAM[0]=D-1
    // @LOOP      // 10: A=4
    // 0;JMP      // 11: jump to LOOP
    // (END)
    // @END       // 12: infinite halt loop
    // 0;JMP      // 13:

    CPUEngine cpu;
    cpu.load_string(
        "0000000000000011\n"   // @3
        "1110110000010000\n"   // D=A
        "0000000000000000\n"   // @0
        "1110001100001000\n"   // M=D
        "0000000000000000\n"   // @0  (LOOP)
        "1111110000010000\n"   // D=M
        "0000000000001100\n"   // @12 (END)
        "1110001100000010\n"   // D;JEQ
        "0000000000000000\n"   // @0
        "1110001110001000\n"   // M=D-1
        "0000000000000100\n"   // @4 (LOOP)
        "1110101010000111\n"   // 0;JMP
        "0000000000001100\n"   // @12 (END)
        "1110101010000111\n"); // 0;JMP

    // Run with limit to avoid infinite halt loop
    cpu.run_for(100);
    check(cpu.read_ram(0) == 0, "decrement loop: RAM[0]=0");
    check(cpu.get_pc() == 12 || cpu.get_pc() == 13, "decrement loop: halted at END");
}

void test_cpu_max_program() {
    // Max(R0, R1) -> R2
    // Standard nand2tetris Max program:
    //
    // @0         // 0
    // D=M        // 1: D=RAM[0]
    // @1         // 2
    // D=D-M      // 3: D=RAM[0]-RAM[1]
    // @10        // 4: @OUTPUT_FIRST
    // D;JGT      // 5: if R0>R1, jump
    // @1         // 6
    // D=M        // 7: D=RAM[1]
    // @12        // 8: @END
    // 0;JMP      // 9: jump to END
    // (OUTPUT_FIRST)
    // @0         // 10
    // D=M        // 11: D=RAM[0]
    // (END)
    // @2         // 12
    // M=D        // 13: RAM[2]=D
    // @14        // 14: infinite loop
    // 0;JMP      // 15

    CPUEngine cpu;
    cpu.load_string(
        "0000000000000000\n"   // @0
        "1111110000010000\n"   // D=M
        "0000000000000001\n"   // @1
        "1111010011010000\n"   // D=D-M
        "0000000000001010\n"   // @10
        "1110001100000001\n"   // D;JGT
        "0000000000000001\n"   // @1
        "1111110000010000\n"   // D=M
        "0000000000001100\n"   // @12
        "1110101010000111\n"   // 0;JMP
        "0000000000000000\n"   // @0
        "1111110000010000\n"   // D=M
        "0000000000000010\n"   // @2
        "1110001100001000\n"   // M=D
        "0000000000001110\n"   // @14
        "1110101010000111\n"); // 0;JMP

    // Test case 1: R0=10, R1=20 => R2 should be 20
    cpu.write_ram(0, 10);
    cpu.write_ram(1, 20);
    cpu.run_for(100);
    check(cpu.read_ram(2) == 20, "Max(10, 20) = 20");

    // Test case 2: R0=30, R1=5 => R2 should be 30
    cpu.reset();
    cpu.load_string(
        "0000000000000000\n"
        "1111110000010000\n"
        "0000000000000001\n"
        "1111010011010000\n"
        "0000000000001010\n"
        "1110001100000001\n"
        "0000000000000001\n"
        "1111110000010000\n"
        "0000000000001100\n"
        "1110101010000111\n"
        "0000000000000000\n"
        "1111110000010000\n"
        "0000000000000010\n"
        "1110001100001000\n"
        "0000000000001110\n"
        "1110101010000111\n");
    cpu.write_ram(0, 30);
    cpu.write_ram(1, 5);
    cpu.run_for(100);
    check(cpu.read_ram(2) == 30, "Max(30, 5) = 30");
}

void test_cpu_breakpoint() {
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000001\n"   // @1
        "1110110000010000\n"   // D=A
        "0000000000000010\n"   // @2
        "1110110000010000\n"); // D=A

    cpu.add_breakpoint(2);
    CPUState state = cpu.run();
    check(state == CPUState::PAUSED, "breakpoint pauses execution");
    check(cpu.get_pause_reason() == CPUPauseReason::BREAKPOINT, "pause reason is BREAKPOINT");
    check(cpu.get_pc() == 2, "paused at breakpoint address");
    check(cpu.get_d() == 1, "D=1 before breakpoint");

    // Continue after breakpoint
    cpu.clear_breakpoints();
    state = cpu.run();
    check(state == CPUState::HALTED, "continues after breakpoint");
    check(cpu.get_d() == 2, "D=2 after continuing");
}

void test_cpu_run_for() {
    // Two-instruction infinite loop: @0 / 0;JMP
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000000\n"   // @0
        "1110101010000111\n"); // 0;JMP

    CPUState state = cpu.run_for(100);
    check(state == CPUState::PAUSED, "run_for pauses at limit");
    check(cpu.get_stats().instructions_executed == 100, "run_for executes exact count");
}

void test_cpu_statistics() {
    // @5 / D=A / @100 / M=D (2 A-instructions, 2 C-instructions, 1 memory write)
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000101\n"   // @5
        "1110110000010000\n"   // D=A
        "0000000001100100\n"   // @100
        "1110001100001000\n"); // M=D
    cpu.run();

    auto& s = cpu.get_stats();
    check(s.instructions_executed == 4, "stats: 4 instructions");
    check(s.a_instruction_count == 2, "stats: 2 A-instructions");
    check(s.c_instruction_count == 2, "stats: 2 C-instructions");
    check(s.memory_writes == 1, "stats: 1 memory write");
    check(s.memory_reads == 0, "stats: 0 memory reads");
}

void test_cpu_step() {
    std::cout << "\n--- Stepping ---\n";

    CPUEngine cpu;
    cpu.load_string(
        "0000000000000101\n"   // @5
        "1110110000010000\n"   // D=A
        "0000000000001010\n"); // @10

    cpu.step();
    check(cpu.get_a() == 5, "step 1: A=5");
    check(cpu.get_pc() == 1, "step 1: PC=1");

    cpu.step();
    check(cpu.get_d() == 5, "step 2: D=5");
    check(cpu.get_pc() == 2, "step 2: PC=2");

    cpu.step();
    check(cpu.get_a() == 10, "step 3: A=10");
    check(cpu.get_state() == CPUState::HALTED, "step 3: halted (end of program)");
}

void test_cpu_dest_am_overlap() {
    std::cout << "\n--- Edge Cases ---\n";

    // Test AM=D+1: both A and M are written.
    // When dest includes A and M, M should be written to RAM[original_A].
    // @100 / D=A / @50 / AM=D+1
    // After: A=101, D still 100, RAM[50]=101
    CPUEngine cpu;
    cpu.load_string(
        "0000000001100100\n"   // @100
        "1110110000010000\n"   // D=A
        "0000000000110010\n"   // @50
        "1110011111101000\n"); // AM=D+1
    cpu.run();
    check(cpu.get_a() == 101, "AM=D+1: A=101");
    check(cpu.read_ram(50) == 101, "AM=D+1: RAM[50]=101 (original A used for M write)");
}

void test_cpu_negative_arithmetic() {
    // Test signed comparison: -1 < 0
    // @1 / D=A / D=-D / @10 / D;JLT
    // D becomes -1 (0xFFFF), which is < 0, so jump should be taken
    CPUEngine cpu;
    cpu.load_string(
        "0000000000000001\n"   // @1
        "1110110000010000\n"   // D=A
        "1110001111010000\n"   // D=-D  (D = -1)
        "0000000000001010\n"   // @10
        "1110001100000100\n"); // D;JLT
    cpu.run();
    check(cpu.get_pc() == 10, "negative jump: D=-1, JLT taken");
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "=== CPU Engine Tests ===\n\n";

    test_decode_a_instruction();
    test_decode_c_instruction();
    test_decode_checked();
    test_disassembly();
    test_memory();
    test_cpu_set_d();
    test_cpu_add();
    test_cpu_write_ram();
    test_cpu_jump_taken();
    test_cpu_jump_not_taken();
    test_cpu_decrement_loop();
    test_cpu_max_program();
    test_cpu_breakpoint();
    test_cpu_run_for();
    test_cpu_statistics();
    test_cpu_step();
    test_cpu_dest_am_overlap();
    test_cpu_negative_arithmetic();

    std::cout << "\n=== " << pass_count << "/" << test_count << " tests passed! ===\n";
    return 0;
}
