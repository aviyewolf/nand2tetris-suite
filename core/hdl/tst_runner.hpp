// ==============================================================================
// TST Script Runner
// ==============================================================================
// Parses and executes .tst test scripts for HDL chip testing.
// Supports: load, output-file, compare-to, output-list, set, eval, output.
// Formats output with pipe-delimited columns matching .cmp format.
// ==============================================================================

#ifndef NAND2TETRIS_TST_RUNNER_HPP
#define NAND2TETRIS_TST_RUNNER_HPP

#include "hdl_chip.hpp"
#include <string>
#include <vector>
#include <memory>

namespace n2t {

// Output column format specification
struct OutputColumn {
    std::string pin_name;
    char mode = 'B';        // B=binary, D=decimal, S=string, X=hex
    int left_pad = 1;
    int width = 1;
    int right_pad = 1;
};

// TST command types
enum class TstCommandType {
    LOAD, OUTPUT_FILE, COMPARE_TO, OUTPUT_LIST,
    SET, EVAL, OUTPUT
};

struct TstCommand {
    TstCommandType type;
    std::string arg1;       // chip name, file name, pin name
    std::string arg2;       // value for SET
    std::vector<OutputColumn> columns;  // for OUTPUT_LIST
    LineNumber source_line = 0;
};

class TstRunner {
public:
    // Parse a test script from string
    void parse(const std::string& source, const std::string& name = "<tst>");

    // Set the chip resolver for loading chips
    void set_chip_resolver(ChipResolver resolver);

    // Set comparison data (contents of .cmp file)
    void set_compare_data(const std::string& cmp_data);

    // Execute next command, returns false when done
    bool step();

    // Execute all commands
    void run_all();

    // Reset to beginning
    void reset();

    // Get current position
    size_t get_position() const { return pos_; }
    size_t get_command_count() const { return commands_.size(); }

    // Get output table
    const std::string& get_output() const { return output_; }

    // Get comparison result
    bool has_comparison_error() const { return !comparison_error_.empty(); }
    const std::string& get_comparison_error() const { return comparison_error_; }

    // Get current chip
    HDLChip* get_chip() { return chip_.get(); }

private:
    void parse_commands(const std::string& source, const std::string& name);
    std::vector<OutputColumn> parse_output_list(const std::string& spec);
    OutputColumn parse_column_spec(const std::string& spec);

    void execute(const TstCommand& cmd);
    void do_load(const std::string& chip_name);
    void do_set(const std::string& pin, const std::string& value);
    void do_eval();
    void do_output();
    void compare_output();

    std::string format_value(int64_t value, const OutputColumn& col) const;
    int64_t parse_value(const std::string& str) const;

    std::vector<TstCommand> commands_;
    size_t pos_ = 0;

    std::unique_ptr<HDLChip> chip_;
    ChipResolver resolver_;

    std::vector<OutputColumn> output_columns_;
    std::string output_;
    std::string output_file_name_;

    std::string compare_data_;
    std::vector<std::string> compare_lines_;
    size_t output_row_ = 0;
    std::string comparison_error_;
    std::string script_name_;
};

}  // namespace n2t

#endif  // NAND2TETRIS_TST_RUNNER_HPP
