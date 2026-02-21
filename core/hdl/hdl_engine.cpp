// ==============================================================================
// HDL Simulation Engine Implementation
// ==============================================================================

#include "hdl_engine.hpp"
#include "hdl_builtins.hpp"
#include "error.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace n2t {

namespace fs = std::filesystem;

HDLEngine::HDLEngine() {
    state_ = HDLState::READY;
}

// ==============================================================================
// Loading
// ==============================================================================

void HDLEngine::load_hdl_file(const std::string& path) {
    try {
        HDLChipDef def = parser_.parse_file(path);
        chip_defs_[def.name] = def;

        // Create the chip instance
        chip_ = resolve_chip(def.name);
        state_ = HDLState::READY;
        error_message_.clear();
    } catch (const N2TError& e) {
        set_error(e.what());
    }
}

void HDLEngine::load_hdl_string(const std::string& source, const std::string& name) {
    try {
        HDLChipDef def = parser_.parse_string(source, name);
        chip_defs_[def.name] = def;

        // Create the chip instance
        chip_ = resolve_chip(def.name);
        state_ = HDLState::READY;
        error_message_.clear();
    } catch (const N2TError& e) {
        set_error(e.what());
    }
}

void HDLEngine::add_search_path(const std::string& dir) {
    search_paths_.push_back(dir);
}

void HDLEngine::reset() {
    if (chip_) chip_->reset();
    stats_.reset();
    state_ = HDLState::READY;
    error_message_.clear();
}

// ==============================================================================
// Direct Chip Manipulation
// ==============================================================================

void HDLEngine::set_input(const std::string& pin, int64_t value) {
    if (!chip_) {
        set_error("No chip loaded");
        return;
    }
    try {
        chip_->set_pin(pin, value);
    } catch (const N2TError& e) {
        set_error(e.what());
    }
}

int64_t HDLEngine::get_output(const std::string& pin) const {
    if (!chip_) return 0;
    return chip_->get_pin(pin);
}

void HDLEngine::eval() {
    if (!chip_) {
        set_error("No chip loaded");
        return;
    }
    try {
        chip_->eval();
        stats_.eval_count++;
    } catch (const N2TError& e) {
        set_error(e.what());
    }
}

// ==============================================================================
// Test Script Execution
// ==============================================================================

HDLState HDLEngine::run_test_string(const std::string& tst, const std::string& cmp,
                                     const std::string& name) {
    try {
        state_ = HDLState::RUNNING;
        error_message_.clear();

        tst_runner_.set_chip_resolver(make_resolver());
        if (!cmp.empty()) {
            tst_runner_.set_compare_data(cmp);
        }
        tst_runner_.parse(tst, name);
        tst_runner_.run_all();

        // Update stats
        if (tst_runner_.get_chip()) {
            chip_ = nullptr;  // Test runner owns the chip
        }

        if (tst_runner_.has_comparison_error()) {
            set_error(tst_runner_.get_comparison_error());
            return state_;
        }

        state_ = HDLState::HALTED;
    } catch (const N2TError& e) {
        set_error(e.what());
    } catch (const std::exception& e) {
        set_error(e.what());
    }
    return state_;
}

HDLState HDLEngine::step_test() {
    try {
        if (state_ == HDLState::ERROR) return state_;

        bool more = tst_runner_.step();
        if (!more) {
            state_ = HDLState::HALTED;
        }

        if (tst_runner_.has_comparison_error()) {
            set_error(tst_runner_.get_comparison_error());
        }
    } catch (const N2TError& e) {
        set_error(e.what());
    }
    return state_;
}

// ==============================================================================
// Output
// ==============================================================================

const std::string& HDLEngine::get_output_table() const {
    return tst_runner_.get_output();
}

bool HDLEngine::has_comparison_error() const {
    return tst_runner_.has_comparison_error();
}

// ==============================================================================
// Chip Resolution
// ==============================================================================

std::unique_ptr<HDLChip> HDLEngine::resolve_chip(const std::string& name) {
    auto resolver = make_resolver();
    return resolver(name);
}

ChipResolver HDLEngine::make_resolver() {
    return [this](const std::string& name) -> std::unique_ptr<HDLChip> {
        // 1. Check builtin registry
        const auto& builtins = get_builtin_registry();
        auto bit = builtins.find(name);
        if (bit != builtins.end()) {
            return std::make_unique<HDLChip>(bit->second.def, bit->second.eval_fn);
        }

        // 2. Check loaded HDL definitions
        auto dit = chip_defs_.find(name);
        if (dit != chip_defs_.end()) {
            return std::make_unique<HDLChip>(dit->second, make_resolver());
        }

        // 3. Search paths for .hdl files
        for (const auto& dir : search_paths_) {
            fs::path hdl_path = fs::path(dir) / (name + ".hdl");
            if (fs::exists(hdl_path)) {
                HDLChipDef def = parser_.parse_file(hdl_path.string());
                chip_defs_[def.name] = def;
                return std::make_unique<HDLChip>(def, make_resolver());
            }
        }

        return nullptr;
    };
}

void HDLEngine::set_error(const std::string& msg) {
    state_ = HDLState::ERROR;
    error_message_ = msg;
}

}  // namespace n2t
