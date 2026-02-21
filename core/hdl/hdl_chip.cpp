// ==============================================================================
// HDL Chip Runtime Implementation
// ==============================================================================

#include "hdl_chip.hpp"
#include "hdl_builtins.hpp"
#include "error.hpp"
#include <algorithm>
#include <queue>
#include <set>

namespace n2t {

// ==============================================================================
// Constructors
// ==============================================================================

HDLChip::HDLChip(const HDLChipDef& def, std::function<void(HDLChip&)> eval_fn)
    : def_(def), builtin_eval_(std::move(eval_fn))
{
    init_pins();
}

HDLChip::HDLChip(const HDLChipDef& def, const ChipResolver& resolver)
    : def_(def)
{
    init_pins();
    build_wiring(resolver);
    compute_eval_order();
}

// ==============================================================================
// Pin Access
// ==============================================================================

void HDLChip::init_pins() {
    for (const auto& p : def_.inputs) {
        pins_[p.name] = 0;
        pin_widths_[p.name] = p.width;
    }
    for (const auto& p : def_.outputs) {
        pins_[p.name] = 0;
        pin_widths_[p.name] = p.width;
    }
}

int64_t HDLChip::get_pin(const std::string& name) const {
    auto it = pins_.find(name);
    if (it == pins_.end()) {
        throw RuntimeError("Unknown pin: '" + name + "' on chip " + def_.name);
    }
    return it->second;
}

void HDLChip::set_pin(const std::string& name, int64_t value) {
    auto it = pins_.find(name);
    if (it == pins_.end()) {
        throw RuntimeError("Unknown pin: '" + name + "' on chip " + def_.name);
    }
    it->second = value;
}

int64_t HDLChip::get_pin_bits(const std::string& name, int lo, int hi) const {
    int64_t val = get_pin(name);
    if (lo < 0) return val;
    int64_t mask = ((int64_t(1) << (hi - lo + 1)) - 1);
    return (val >> lo) & mask;
}

void HDLChip::set_pin_bits(const std::string& name, int lo, int hi, int64_t value) {
    if (lo < 0) {
        set_pin(name, value);
        return;
    }
    int64_t current = get_pin(name);
    int width = hi - lo + 1;
    int64_t mask = ((int64_t(1) << width) - 1);
    value &= mask;
    current &= ~(mask << lo);
    current |= (value << lo);
    set_pin(name, current);
}

uint8_t HDLChip::get_pin_width(const std::string& name) const {
    auto it = pin_widths_.find(name);
    if (it == pin_widths_.end()) return 0;
    return it->second;
}

void HDLChip::reset() {
    for (auto& [name, val] : pins_) {
        val = 0;
    }
    for (auto& sub : sub_chips_) {
        sub->reset();
    }
}

// ==============================================================================
// Evaluation
// ==============================================================================

void HDLChip::eval() {
    if (builtin_eval_) {
        builtin_eval_(*this);
        return;
    }

    // User-defined chip: propagate inputs, eval parts in order, collect outputs
    for (size_t idx : eval_order_) {
        propagate_inputs(idx);
        sub_chips_[idx]->eval();
        collect_outputs(idx);
    }
}

// ==============================================================================
// Wiring & Topological Sort
// ==============================================================================

bool HDLChip::is_chip_input(const std::string& name) const {
    for (const auto& p : def_.inputs) {
        if (p.name == name) return true;
    }
    return false;
}

bool HDLChip::is_chip_output(const std::string& name) const {
    for (const auto& p : def_.outputs) {
        if (p.name == name) return true;
    }
    return false;
}

void HDLChip::build_wiring(const ChipResolver& resolver) {
    // Create sub-chip instances
    for (size_t pi = 0; pi < def_.parts.size(); pi++) {
        const auto& part = def_.parts[pi];
        auto sub = resolver(part.chip_name);
        if (!sub) {
            throw RuntimeError("Unknown chip: '" + part.chip_name +
                               "' at line " + std::to_string(part.source_line));
        }
        sub_chips_.push_back(std::move(sub));

        // Process connections
        for (const auto& conn : part.connections) {
            const auto& internal = conn.internal;  // part's pin
            const auto& external = conn.external;   // chip's pin/wire/constant

            // Determine if internal pin is an input or output of the sub-chip
            bool is_part_input = false;
            const auto& sub_def = sub_chips_.back()->get_def();
            for (const auto& inp : sub_def.inputs) {
                if (inp.name == internal.name) { is_part_input = true; break; }
            }

            // Handle true/false constants
            if (external.name == "true" || external.name == "false") {
                int64_t val = (external.name == "true") ? 1 : 0;
                // For true on a 16-bit pin, use 0xFFFF
                if (external.name == "true") {
                    uint8_t w = sub_chips_.back()->get_pin_width(internal.name);
                    if (w > 1) val = (int64_t(1) << w) - 1;
                }
                WireMapping m;
                m.part_index = pi;
                m.part_pin = internal.name;
                m.part_lo = internal.lo;
                m.part_hi = internal.hi;
                m.chip_pin = external.name;
                m.chip_lo = external.lo;
                m.chip_hi = external.hi;
                m.is_input = true;

                // Store constant directly on the sub-chip pin
                sub_chips_.back()->set_pin_bits(internal.name, internal.lo, internal.hi, val);
                // We still track it but mark as constant (chip_pin = true/false)
                input_mappings_.push_back(m);
                continue;
            }

            // Ensure internal wires exist in our pin map
            if (!is_chip_input(external.name) && !is_chip_output(external.name)) {
                if (pins_.find(external.name) == pins_.end()) {
                    // Internal wire - determine width from context
                    uint8_t w = sub_chips_.back()->get_pin_width(internal.name);
                    pins_[external.name] = 0;
                    pin_widths_[external.name] = w;
                }
            }

            WireMapping m;
            m.part_index = pi;
            m.part_pin = internal.name;
            m.part_lo = internal.lo;
            m.part_hi = internal.hi;
            m.chip_pin = external.name;
            m.chip_lo = external.lo;
            m.chip_hi = external.hi;
            m.is_input = is_part_input;

            if (is_part_input) {
                input_mappings_.push_back(m);
            } else {
                output_mappings_.push_back(m);
            }
        }
    }
}

void HDLChip::compute_eval_order() {
    size_t n = sub_chips_.size();
    if (n == 0) return;

    // Build dependency graph: which parts produce wires, which consume them
    // part_index -> set of wire names it writes to
    std::vector<std::set<std::string>> part_outputs(n);
    // part_index -> set of wire names it reads from
    std::vector<std::set<std::string>> part_inputs(n);

    for (const auto& m : output_mappings_) {
        part_outputs[m.part_index].insert(m.chip_pin);
    }
    for (const auto& m : input_mappings_) {
        if (m.chip_pin == "true" || m.chip_pin == "false") continue;
        if (!is_chip_input(m.chip_pin) && !is_chip_output(m.chip_pin)) {
            // This reads from an internal wire
            part_inputs[m.part_index].insert(m.chip_pin);
        }
    }

    // Build adjacency: part A must come before part B if A writes a wire B reads
    std::vector<std::vector<size_t>> adj(n);
    std::vector<size_t> in_degree(n, 0);

    for (size_t b = 0; b < n; b++) {
        for (const auto& wire : part_inputs[b]) {
            for (size_t a = 0; a < n; a++) {
                if (a != b && part_outputs[a].count(wire)) {
                    adj[a].push_back(b);
                    in_degree[b]++;
                }
            }
        }
    }

    // Kahn's algorithm
    std::queue<size_t> q;
    for (size_t i = 0; i < n; i++) {
        if (in_degree[i] == 0) q.push(i);
    }

    eval_order_.clear();
    while (!q.empty()) {
        size_t u = q.front();
        q.pop();
        eval_order_.push_back(u);
        for (size_t v : adj[u]) {
            if (--in_degree[v] == 0) q.push(v);
        }
    }

    // If we couldn't order all parts, there's a cycle (shouldn't happen for combinational)
    if (eval_order_.size() != n) {
        // Fall back to sequential order
        eval_order_.clear();
        for (size_t i = 0; i < n; i++) {
            eval_order_.push_back(i);
        }
    }
}

void HDLChip::propagate_inputs(size_t part_index) {
    for (const auto& m : input_mappings_) {
        if (m.part_index != part_index) continue;
        if (m.chip_pin == "true" || m.chip_pin == "false") {
            // Constant - re-apply each eval to be safe
            int64_t val = (m.chip_pin == "true") ? 1 : 0;
            if (m.chip_pin == "true") {
                uint8_t w = sub_chips_[part_index]->get_pin_width(m.part_pin);
                if (w > 1) val = (int64_t(1) << w) - 1;
            }
            sub_chips_[part_index]->set_pin_bits(m.part_pin, m.part_lo, m.part_hi, val);
            continue;
        }

        int64_t val = get_pin_bits(m.chip_pin, m.chip_lo, m.chip_hi);
        sub_chips_[part_index]->set_pin_bits(m.part_pin, m.part_lo, m.part_hi, val);
    }
}

void HDLChip::collect_outputs(size_t part_index) {
    for (const auto& m : output_mappings_) {
        if (m.part_index != part_index) continue;
        int64_t val = sub_chips_[part_index]->get_pin_bits(m.part_pin, m.part_lo, m.part_hi);
        set_pin_bits(m.chip_pin, m.chip_lo, m.chip_hi, val);
    }
}

}  // namespace n2t
