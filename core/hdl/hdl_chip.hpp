// ==============================================================================
// HDL Chip Runtime
// ==============================================================================
// Runtime chip instances with pin storage, evaluation, and bus operations.
// Supports both built-in chips (eval callback) and user-defined chips
// (sub-chip instances evaluated in topological order).
// ==============================================================================

#ifndef NAND2TETRIS_HDL_CHIP_HPP
#define NAND2TETRIS_HDL_CHIP_HPP

#include "hdl_parser.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace n2t {

class HDLChip;

// Callback type for resolving chip names to chip definitions + builtins
using ChipResolver = std::function<std::unique_ptr<HDLChip>(const std::string&)>;

class HDLChip {
public:
    // Construct a built-in chip
    HDLChip(const HDLChipDef& def, std::function<void(HDLChip&)> eval_fn);

    // Construct a user-defined chip from its definition, resolving sub-parts
    HDLChip(const HDLChipDef& def, const ChipResolver& resolver);

    // Pin access
    int64_t get_pin(const std::string& name) const;
    void set_pin(const std::string& name, int64_t value);

    // Sub-bus access
    int64_t get_pin_bits(const std::string& name, int lo, int hi) const;
    void set_pin_bits(const std::string& name, int lo, int hi, int64_t value);

    // Evaluate the chip
    void eval();

    // Get chip definition
    const HDLChipDef& get_def() const { return def_; }

    // Get pin width
    uint8_t get_pin_width(const std::string& name) const;

    // Reset all pins to 0
    void reset();

private:
    HDLChipDef def_;
    std::unordered_map<std::string, int64_t> pins_;
    std::unordered_map<std::string, uint8_t> pin_widths_;

    // Built-in eval
    std::function<void(HDLChip&)> builtin_eval_;

    // User-defined chip internals
    struct WireMapping {
        size_t part_index;          // index into sub_chips_
        std::string part_pin;       // pin name on the sub-chip
        int part_lo, part_hi;       // sub-range on part pin (-1 = full)
        std::string chip_pin;       // pin/wire name on this chip
        int chip_lo, chip_hi;       // sub-range on chip pin (-1 = full)
        bool is_input;              // true = chip->part, false = part->chip
    };

    std::vector<std::unique_ptr<HDLChip>> sub_chips_;
    std::vector<size_t> eval_order_;    // topological order of sub-chips
    std::vector<WireMapping> input_mappings_;   // chip pins -> part pins
    std::vector<WireMapping> output_mappings_;  // part pins -> chip pins

    void init_pins();
    void build_wiring(const ChipResolver& resolver);
    void compute_eval_order();
    void propagate_inputs(size_t part_index);
    void collect_outputs(size_t part_index);

    bool is_chip_input(const std::string& name) const;
    bool is_chip_output(const std::string& name) const;
};

}  // namespace n2t

#endif  // NAND2TETRIS_HDL_CHIP_HPP
