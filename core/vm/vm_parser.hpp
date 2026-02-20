// ==============================================================================
// VM Parser
// ==============================================================================
// Parses .vm files and converts them to VMCommand structures.
// The parser handles:
// - Reading .vm files line by line
// - Removing comments and whitespace
// - Parsing each command type
// - Reporting errors with helpful messages
// ==============================================================================

#ifndef NAND2TETRIS_VM_PARSER_HPP
#define NAND2TETRIS_VM_PARSER_HPP

#include "vm_command.hpp"
#include "error.hpp"
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <filesystem>

namespace n2t {

// ==============================================================================
// Parsed VM Program
// ==============================================================================

/**
 * @brief A complete parsed VM program
 *
 * A VM program consists of one or more .vm files.
 * Each file may define multiple functions.
 *
 * The program stores:
 * - All commands in order of execution
 * - Source file information for debugging
 * - Label positions for jumps
 * - Function entry points
 */
struct VMProgram {
    // All commands from all loaded files, in order
    std::vector<VMCommand> commands;

    // Maps label names to command indices (for goto/if-goto)
    // Label names are scoped: "functionName$labelName"
    std::unordered_map<std::string, size_t> label_positions;

    // Maps function names to command indices (for call)
    std::unordered_map<std::string, size_t> function_entry_points;

    // Source files that were loaded (for debugging)
    std::vector<std::string> source_files;
};

// ==============================================================================
// VM Parser Class
// ==============================================================================

/**
 * @brief Parses VM source code into executable commands
 *
 * The parser reads .vm files and produces a VMProgram that can be
 * executed by the VM engine.
 *
 * Usage:
 *   VMParser parser;
 *   parser.parse_file("Main.vm");
 *   parser.parse_file("Math.vm");
 *   VMProgram program = parser.get_program();
 *
 * The parser is lenient with whitespace and handles:
 * - Single-line comments: // comment
 * - Empty lines
 * - Leading/trailing whitespace
 * - Multiple spaces between tokens
 */
class VMParser {
public:
    VMParser() = default;

    // =========================================================================
    // File Loading
    // =========================================================================

    /**
     * @brief Parse a single .vm file
     *
     * Reads the file and adds its commands to the program.
     * Multiple files can be parsed to build a complete program.
     *
     * @param file_path Path to the .vm file
     * @throws FileError if file cannot be read
     * @throws ParseError if file contains invalid VM code
     */
    void parse_file(const std::string& file_path);

    /**
     * @brief Parse VM code from a string
     *
     * Useful for testing or parsing code from memory.
     *
     * @param source The VM source code
     * @param file_name Name to use in error messages (default: "<string>")
     * @throws ParseError if source contains invalid VM code
     */
    void parse_string(const std::string& source,
                      const std::string& file_name = "<string>");

    /**
     * @brief Parse all .vm files in a directory
     *
     * Loads all .vm files in the directory alphabetically.
     * This is how you load a multi-file Jack program.
     *
     * @param directory_path Path to directory containing .vm files
     * @throws FileError if directory cannot be read
     * @throws ParseError if any file contains invalid VM code
     */
    void parse_directory(const std::string& directory_path);

    // =========================================================================
    // Program Access
    // =========================================================================

    /**
     * @brief Get the parsed program
     *
     * Returns the complete program with all parsed commands.
     * Call this after parsing all source files.
     *
     * @return The parsed VM program
     */
    VMProgram get_program() const;

    /**
     * @brief Check if any files have been parsed
     */
    bool has_content() const { return !commands_.empty(); }

    /**
     * @brief Clear the parser and start fresh
     */
    void reset();

private:
    // =========================================================================
    // Internal Parsing State
    // =========================================================================

    // Commands accumulated from all parsed files
    std::vector<VMCommand> commands_;

    // Label and function positions
    std::unordered_map<std::string, size_t> label_positions_;
    std::unordered_map<std::string, size_t> function_entry_points_;

    // Source files loaded
    std::vector<std::string> source_files_;

    // Current parsing context
    std::string current_file_;       // File being parsed
    std::string current_function_;   // Current function (for label scoping)
    LineNumber current_line_;        // Line number for error messages

    // =========================================================================
    // Parsing Helpers
    // =========================================================================

    /**
     * @brief Parse a single line of VM code
     *
     * @param line The line to parse (may be empty or comment-only)
     * @return The parsed command, or nullopt if line is empty/comment
     */
    std::optional<VMCommand> parse_line(const std::string& line);

    /**
     * @brief Remove comments and trim whitespace
     *
     * @param line The raw line from the file
     * @return The cleaned line (empty if comment-only)
     */
    std::string clean_line(const std::string& line);

    /**
     * @brief Split a line into tokens (words)
     *
     * @param line The cleaned line
     * @return Vector of tokens
     */
    std::vector<std::string> tokenize(const std::string& line);

    /**
     * @brief Parse an arithmetic command (add, sub, neg, etc.)
     */
    ArithmeticCommand parse_arithmetic(const std::string& op);

    /**
     * @brief Parse a push command
     */
    PushCommand parse_push(const std::vector<std::string>& tokens);

    /**
     * @brief Parse a pop command
     */
    PopCommand parse_pop(const std::vector<std::string>& tokens);

    /**
     * @brief Parse a segment name string to SegmentType
     *
     * @param segment_str The segment name (e.g., "local", "argument")
     * @return The corresponding SegmentType
     * @throws ParseError if segment name is invalid
     */
    SegmentType parse_segment(const std::string& segment_str);

    /**
     * @brief Parse an index string to integer
     *
     * @param index_str The index as a string
     * @return The index as uint16_t
     * @throws ParseError if index is invalid or out of range
     */
    uint16_t parse_index(const std::string& index_str);

    /**
     * @brief Parse a label command
     */
    LabelCommand parse_label(const std::vector<std::string>& tokens);

    /**
     * @brief Parse a goto command
     */
    GotoCommand parse_goto(const std::vector<std::string>& tokens);

    /**
     * @brief Parse an if-goto command
     */
    IfGotoCommand parse_if_goto(const std::vector<std::string>& tokens);

    /**
     * @brief Parse a function command
     */
    FunctionCommand parse_function(const std::vector<std::string>& tokens);

    /**
     * @brief Parse a call command
     */
    CallCommand parse_call(const std::vector<std::string>& tokens);

    /**
     * @brief Parse a return command
     */
    ReturnCommand parse_return();

    /**
     * @brief Create a scoped label name (function$label)
     *
     * Labels in VM are scoped to their containing function.
     * This creates the full scoped name for label lookup.
     */
    std::string make_scoped_label(const std::string& label) const;

    /**
     * @brief Report a parse error with context
     *
     * @param message Error description
     * @throws ParseError always
     */
    [[noreturn]] void error(const std::string& message);

    /**
     * @brief Report a parse error with suggestion
     *
     * @param message Error description
     * @param suggestion What the user might have meant
     * @throws ParseError always
     */
    [[noreturn]] void error_with_suggestion(const std::string& message,
                                             const std::string& wrong,
                                             const std::string& correct);
};

// ==============================================================================
// Utility Functions
// ==============================================================================

/**
 * @brief Check if a string is a valid VM identifier
 *
 * Valid identifiers start with a letter or underscore and contain
 * only letters, digits, underscores, and dots.
 */
bool is_valid_identifier(const std::string& str);

/**
 * @brief Check if a string is a valid label name
 *
 * Labels can contain letters, digits, underscores, dots, and colons.
 */
bool is_valid_label(const std::string& str);

/**
 * @brief Get the base name of a file (without directory and extension)
 *
 * Example: "/path/to/Math.vm" -> "Math"
 */
std::string get_file_basename(const std::string& file_path);

}  // namespace n2t

#endif  // NAND2TETRIS_VM_PARSER_HPP
