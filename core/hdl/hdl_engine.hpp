// ==============================================================================
// HDL Simulation Engine
// ==============================================================================
// Main orchestrator for HDL simulation: chip loading, evaluation, test scripts.
// Follows the engine pattern used by CPU, VM, and Jack engines.
// ==============================================================================

#ifndef NAND2TETRIS_HDL_ENGINE_HPP
#define NAND2TETRIS_HDL_ENGINE_HPP

#include "hdl_parser.hpp"
#include "hdl_chip.hpp"
#include "tst_runner.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace n2t {

enum class HDLState {
    READY,
    RUNNING,
    PAUSED,
    HALTED,
    ERROR
};

struct HDLStats {
    uint64_t eval_count = 0;
    uint64_t output_rows = 0;
    void reset() { eval_count = 0; output_rows = 0; }
};

class HDLEngine {
public:
    HDLEngine();

    // Loading
    void load_hdl_file(const std::string& path);
    void load_hdl_string(const std::string& source, const std::string& name = "<hdl>");
    void add_search_path(const std::string& dir);
    void reset();

    // Direct chip manipulation
    void set_input(const std::string& pin, int64_t value);
    int64_t get_output(const std::string& pin) const;
    void eval();

    // Test script execution
    HDLState run_test_string(const std::string& tst, const std::string& cmp = "",
                             const std::string& name = "<tst>");
    HDLState step_test();

    // Output & comparison
    const std::string& get_output_table() const;
    bool has_comparison_error() const;

    // State
    HDLState get_state() const { return state_; }
    const HDLStats& get_stats() const { return stats_; }
    const std::string& get_error_message() const { return error_message_; }

private:
    // Chip resolution
    std::unique_ptr<HDLChip> resolve_chip(const std::string& name);
    ChipResolver make_resolver();

    // State
    HDLState state_ = HDLState::READY;
    HDLStats stats_;
    std::string error_message_;

    // Loaded chip definitions (from HDL strings/files)
    std::unordered_map<std::string, HDLChipDef> chip_defs_;
    HDLParser parser_;

    // Current chip instance
    std::unique_ptr<HDLChip> chip_;

    // Test runner
    TstRunner tst_runner_;

    // Search paths for .hdl files
    std::vector<std::string> search_paths_;

    void set_error(const std::string& msg);
};

}  // namespace n2t

#endif  // NAND2TETRIS_HDL_ENGINE_HPP
