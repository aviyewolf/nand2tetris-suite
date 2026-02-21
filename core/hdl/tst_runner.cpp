// ==============================================================================
// TST Script Runner Implementation
// ==============================================================================

#include "tst_runner.hpp"
#include "error.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cctype>

namespace n2t {

// ==============================================================================
// Parsing
// ==============================================================================

void TstRunner::parse(const std::string& source, const std::string& name) {
    script_name_ = name;
    commands_.clear();
    pos_ = 0;
    output_.clear();
    output_row_ = 0;
    comparison_error_.clear();
    clock_cycle_ = 0;
    in_tick_phase_ = false;
    parse_commands(source, name);
}

void TstRunner::set_chip_resolver(ChipResolver resolver) {
    resolver_ = std::move(resolver);
}

void TstRunner::set_compare_data(const std::string& cmp_data) {
    compare_data_ = cmp_data;
    compare_lines_.clear();

    // Split into lines, skip empty and header lines
    std::istringstream iss(cmp_data);
    std::string line;
    while (std::getline(iss, line)) {
        // Trim trailing whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty()) {
            compare_lines_.push_back(line);
        }
    }
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void TstRunner::parse_commands(const std::string& source, const std::string& name) {
    // Tokenize by semicolons and commas (commands end with ; or ,)
    // But first strip comments
    std::string clean;
    clean.reserve(source.size());
    size_t i = 0;
    while (i < source.size()) {
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n') i++;
            continue;
        }
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
            i += 2;
            while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) i++;
            if (i + 1 < source.size()) i += 2;
            continue;
        }
        clean += source[i];
        i++;
    }

    // Split on semicolons and commas (both are command terminators in .tst)
    LineNumber line = 1;
    std::string current;
    for (char c : clean) {
        if (c == '\n') line++;
        if (c == ';' || c == ',') {
            std::string cmd_str = trim(current);
            if (!cmd_str.empty()) {
                TstCommand cmd;
                cmd.source_line = line;

                // Parse the command
                std::istringstream cs(cmd_str);
                std::string keyword;
                cs >> keyword;

                if (keyword == "load") {
                    cmd.type = TstCommandType::LOAD;
                    cs >> cmd.arg1;
                    // Remove .hdl extension if present
                    if (cmd.arg1.size() > 4 &&
                        cmd.arg1.substr(cmd.arg1.size() - 4) == ".hdl") {
                        cmd.arg1 = cmd.arg1.substr(0, cmd.arg1.size() - 4);
                    }
                } else if (keyword == "output-file") {
                    cmd.type = TstCommandType::OUTPUT_FILE;
                    cs >> cmd.arg1;
                } else if (keyword == "compare-to") {
                    cmd.type = TstCommandType::COMPARE_TO;
                    cs >> cmd.arg1;
                } else if (keyword == "output-list") {
                    cmd.type = TstCommandType::OUTPUT_LIST;
                    // Rest of line is the column specs
                    std::string rest;
                    std::getline(cs, rest);
                    rest = trim(rest);
                    cmd.columns = parse_output_list(rest);
                } else if (keyword == "set") {
                    cmd.type = TstCommandType::SET;
                    cs >> cmd.arg1;
                    // Value is the rest
                    std::string val;
                    cs >> val;
                    // Could be more tokens for the value
                    std::string extra;
                    while (cs >> extra) val += extra;
                    cmd.arg2 = trim(val);
                } else if (keyword == "eval") {
                    cmd.type = TstCommandType::EVAL;
                } else if (keyword == "tick") {
                    cmd.type = TstCommandType::TICK;
                } else if (keyword == "tock") {
                    cmd.type = TstCommandType::TOCK;
                } else if (keyword == "output") {
                    cmd.type = TstCommandType::OUTPUT;
                } else {
                    throw ParseError(name, line,
                                     "Unknown test command: '" + keyword + "'");
                }

                commands_.push_back(cmd);
            }
            current.clear();
        } else {
            current += c;
        }
    }
}

std::vector<OutputColumn> TstRunner::parse_output_list(const std::string& spec) {
    std::vector<OutputColumn> cols;
    std::istringstream iss(spec);
    std::string tok;
    while (iss >> tok) {
        cols.push_back(parse_column_spec(tok));
    }
    return cols;
}

OutputColumn TstRunner::parse_column_spec(const std::string& spec) {
    OutputColumn col;

    // Format: pinName%Mode.LeftPad.Width.RightPad
    auto pct = spec.find('%');
    if (pct == std::string::npos) {
        col.pin_name = spec;
        return col;
    }

    col.pin_name = spec.substr(0, pct);
    std::string fmt = spec.substr(pct + 1);

    // Parse mode character
    if (!fmt.empty()) {
        col.mode = static_cast<char>(std::toupper(static_cast<unsigned char>(fmt[0])));
        fmt = fmt.substr(1);
    }

    // Parse dot-separated numbers
    std::vector<int> nums;
    std::string num_str;
    for (char c : fmt) {
        if (c == '.') {
            if (!num_str.empty()) {
                nums.push_back(std::stoi(num_str));
                num_str.clear();
            }
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            num_str += c;
        }
    }
    if (!num_str.empty()) nums.push_back(std::stoi(num_str));

    if (nums.size() >= 1) col.left_pad = nums[0];
    if (nums.size() >= 2) col.width = nums[1];
    if (nums.size() >= 3) col.right_pad = nums[2];

    return col;
}

// ==============================================================================
// Execution
// ==============================================================================

bool TstRunner::step() {
    if (pos_ >= commands_.size()) return false;
    execute(commands_[pos_]);
    pos_++;
    return pos_ < commands_.size();
}

void TstRunner::run_all() {
    while (pos_ < commands_.size()) {
        execute(commands_[pos_]);
        pos_++;
        if (!comparison_error_.empty()) return;
    }
}

void TstRunner::reset() {
    pos_ = 0;
    output_.clear();
    output_row_ = 0;
    comparison_error_.clear();
    clock_cycle_ = 0;
    in_tick_phase_ = false;
    if (chip_) chip_->reset();
}

void TstRunner::execute(const TstCommand& cmd) {
    switch (cmd.type) {
        case TstCommandType::LOAD:
            do_load(cmd.arg1);
            break;
        case TstCommandType::OUTPUT_FILE:
            output_file_name_ = cmd.arg1;
            break;
        case TstCommandType::COMPARE_TO:
            // Compare-to just sets the name; actual data set via set_compare_data
            break;
        case TstCommandType::OUTPUT_LIST:
            output_columns_ = cmd.columns;
            // Emit header row
            {
                std::string header = "|";
                for (const auto& col : output_columns_) {
                    int total = col.left_pad + col.width + col.right_pad;
                    int name_len = static_cast<int>(col.pin_name.size());
                    int pad_left = (total - name_len) / 2;
                    int pad_right = total - name_len - pad_left;
                    if (pad_left < 0) pad_left = 0;
                    if (pad_right < 0) pad_right = 0;
                    header += std::string(static_cast<size_t>(pad_left), ' ');
                    header += col.pin_name;
                    header += std::string(static_cast<size_t>(pad_right), ' ');
                    header += "|";
                }
                output_ += header + "\n";
            }
            break;
        case TstCommandType::SET:
            do_set(cmd.arg1, cmd.arg2);
            break;
        case TstCommandType::EVAL:
            do_eval();
            break;
        case TstCommandType::TICK:
            do_tick();
            break;
        case TstCommandType::TOCK:
            do_tock();
            break;
        case TstCommandType::OUTPUT:
            do_output();
            break;
    }
}

void TstRunner::do_load(const std::string& chip_name) {
    if (!resolver_) {
        throw RuntimeError("No chip resolver set for test runner");
    }
    chip_ = resolver_(chip_name);
    if (!chip_) {
        throw RuntimeError("Could not load chip: '" + chip_name + "'");
    }
}

void TstRunner::do_set(const std::string& pin, const std::string& value) {
    if (!chip_) {
        throw RuntimeError("No chip loaded");
    }

    // Check for sub-bus notation: pin[i] or pin[i..j]
    auto bracket = pin.find('[');
    if (bracket != std::string::npos) {
        std::string base = pin.substr(0, bracket);
        std::string range = pin.substr(bracket + 1);
        range.pop_back(); // remove ']'
        auto dotdot = range.find("..");
        if (dotdot != std::string::npos) {
            int lo = std::stoi(range.substr(0, dotdot));
            int hi = std::stoi(range.substr(dotdot + 2));
            chip_->set_pin_bits(base, lo, hi, parse_value(value));
        } else {
            int bit = std::stoi(range);
            chip_->set_pin_bits(base, bit, bit, parse_value(value));
        }
    } else {
        chip_->set_pin(pin, parse_value(value));
    }
}

void TstRunner::do_eval() {
    if (!chip_) {
        throw RuntimeError("No chip loaded");
    }
    chip_->eval();
}

void TstRunner::do_tick() {
    if (!chip_) {
        throw RuntimeError("No chip loaded");
    }
    in_tick_phase_ = true;
    chip_->tick();
}

void TstRunner::do_tock() {
    if (!chip_) {
        throw RuntimeError("No chip loaded");
    }
    in_tick_phase_ = false;
    clock_cycle_++;
    chip_->tock();
}

void TstRunner::do_output() {
    if (!chip_) {
        throw RuntimeError("No chip loaded");
    }

    std::string row = "|";
    for (const auto& col : output_columns_) {
        // Handle 'time' pseudo-pin
        if (col.pin_name == "time") {
            std::string time_str;
            if (in_tick_phase_) {
                time_str = std::to_string(clock_cycle_) + "+";
            } else {
                time_str = std::to_string(clock_cycle_);
            }
            // Right-justify within width, apply padding
            int total_width = col.width;
            while (static_cast<int>(time_str.size()) < total_width) {
                time_str = " " + time_str;
            }
            row += std::string(static_cast<size_t>(col.left_pad), ' ');
            row += time_str;
            row += std::string(static_cast<size_t>(col.right_pad), ' ');
            row += "|";
            continue;
        }

        // Check for sub-bus in pin name
        int64_t val;
        auto bracket = col.pin_name.find('[');
        if (bracket != std::string::npos) {
            std::string base = col.pin_name.substr(0, bracket);
            std::string range = col.pin_name.substr(bracket + 1);
            if (!range.empty() && range.back() == ']') range.pop_back();
            auto dotdot = range.find("..");
            if (dotdot != std::string::npos) {
                int lo = std::stoi(range.substr(0, dotdot));
                int hi = std::stoi(range.substr(dotdot + 2));
                val = chip_->get_pin_bits(base, lo, hi);
            } else {
                int bit = std::stoi(range);
                val = chip_->get_pin_bits(base, bit, bit);
            }
        } else {
            val = chip_->get_pin(col.pin_name);
        }

        row += format_value(val, col);
        row += "|";
    }
    output_ += row + "\n";
    output_row_++;

    // Compare if we have comparison data
    compare_output();
}

void TstRunner::compare_output() {
    if (compare_lines_.empty()) return;

    // The compare lines include header, so data starts at index 1
    size_t cmp_index = output_row_;  // output_row_ is 1-based after increment
    if (cmp_index >= compare_lines_.size()) return;

    // Get the last output row
    std::string actual_row;
    {
        // Find the last line in output_
        size_t last_nl = output_.rfind('\n', output_.size() - 2);
        if (last_nl == std::string::npos) {
            actual_row = output_.substr(0, output_.size() - 1);
        } else {
            actual_row = output_.substr(last_nl + 1, output_.size() - last_nl - 2);
        }
    }

    const std::string& expected = compare_lines_[cmp_index];

    if (actual_row != expected) {
        comparison_error_ = "Comparison failure at line " +
                            std::to_string(cmp_index + 1) +
                            ":\nExpected: " + expected +
                            "\n  Actual: " + actual_row;
    }
}

std::string TstRunner::format_value(int64_t value, const OutputColumn& col) const {
    std::string formatted;

    switch (col.mode) {
        case 'B': {
            // Binary format
            std::string bits;
            int w = col.width;
            for (int b = w - 1; b >= 0; b--) {
                bits += ((value >> b) & 1) ? '1' : '0';
            }
            formatted = bits;
            break;
        }
        case 'D': {
            // Decimal format (signed for 16-bit values)
            int16_t signed_val = static_cast<int16_t>(static_cast<uint16_t>(value & 0xFFFF));
            formatted = std::to_string(signed_val);
            // Right-justify within width
            while (static_cast<int>(formatted.size()) < col.width) {
                formatted = " " + formatted;
            }
            break;
        }
        case 'X': {
            // Hexadecimal
            std::ostringstream oss;
            oss << std::hex << (value & 0xFFFF);
            formatted = oss.str();
            while (static_cast<int>(formatted.size()) < col.width) {
                formatted = "0" + formatted;
            }
            break;
        }
        case 'S': {
            // String (just the value as-is)
            formatted = std::to_string(value);
            break;
        }
        default:
            formatted = std::to_string(value);
            break;
    }

    // Apply padding
    std::string result;
    result += std::string(static_cast<size_t>(col.left_pad), ' ');
    result += formatted;
    result += std::string(static_cast<size_t>(col.right_pad), ' ');
    return result;
}

int64_t TstRunner::parse_value(const std::string& str) const {
    if (str.empty()) return 0;

    // Binary: %Bxxxx
    if (str.size() > 2 && str[0] == '%' &&
        (str[1] == 'B' || str[1] == 'b')) {
        int64_t val = 0;
        for (size_t i = 2; i < str.size(); i++) {
            val = (val << 1) | (str[i] == '1' ? 1 : 0);
        }
        return val;
    }

    // Hex: %Xxxxx
    if (str.size() > 2 && str[0] == '%' &&
        (str[1] == 'X' || str[1] == 'x')) {
        return std::stoll(str.substr(2), nullptr, 16);
    }

    // Decimal (possibly negative)
    return std::stoll(str);
}

}  // namespace n2t
