// ==============================================================================
// HDL Built-in Chips
// ==============================================================================
// Provides built-in implementations of all combinational chips from
// nand2tetris projects 01-02: gates, mux/dmux, 16-bit, multi-way, ALU.
// ==============================================================================

#ifndef NAND2TETRIS_HDL_BUILTINS_HPP
#define NAND2TETRIS_HDL_BUILTINS_HPP

#include "hdl_parser.hpp"
#include <functional>
#include <unordered_map>

namespace n2t {

// Forward declaration
class HDLChip;

struct BuiltinInfo {
    HDLChipDef def;
    std::function<void(HDLChip&)> eval_fn;
};

// Registry of all built-in chips
const std::unordered_map<std::string, BuiltinInfo>& get_builtin_registry();

// Individual eval functions (exposed for testing)
void eval_Nand(HDLChip& chip);
void eval_Not(HDLChip& chip);
void eval_And(HDLChip& chip);
void eval_Or(HDLChip& chip);
void eval_Xor(HDLChip& chip);
void eval_Mux(HDLChip& chip);
void eval_DMux(HDLChip& chip);
void eval_Not16(HDLChip& chip);
void eval_And16(HDLChip& chip);
void eval_Or16(HDLChip& chip);
void eval_Mux16(HDLChip& chip);
void eval_Or8Way(HDLChip& chip);
void eval_Mux4Way16(HDLChip& chip);
void eval_Mux8Way16(HDLChip& chip);
void eval_DMux4Way(HDLChip& chip);
void eval_DMux8Way(HDLChip& chip);
void eval_HalfAdder(HDLChip& chip);
void eval_FullAdder(HDLChip& chip);
void eval_Add16(HDLChip& chip);
void eval_Inc16(HDLChip& chip);
void eval_ALU(HDLChip& chip);

}  // namespace n2t

#endif  // NAND2TETRIS_HDL_BUILTINS_HPP
