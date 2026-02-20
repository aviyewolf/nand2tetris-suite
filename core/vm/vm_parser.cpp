// ==============================================================================
// VM Parser Implementation
// ==============================================================================

#include "vm_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace n2t {

// ==============================================================================
// File Loading
// ==============================================================================

void VMParser::parse_file(const std::string& file_path) {
    // Open the file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw FileError(file_path, "Could not open file for reading");
    }

    // Read entire file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Get just the filename for error messages and static variable naming
    current_file_ = get_file_basename(file_path);
    source_files_.push_back(file_path);

    // Parse the content
    parse_string(content, current_file_);
}

void VMParser::parse_string(const std::string& source, const std::string& file_name) {
    current_file_ = file_name;
    current_line_ = 0;

    // Parse line by line
    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        current_line_++;

        // Try to parse this line
        auto command = parse_line(line);
        if (command.has_value()) {
            commands_.push_back(command.value());
        }
    }
}

void VMParser::parse_directory(const std::string& directory_path) {
    namespace fs = std::filesystem;

    // Check if directory exists
    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        throw FileError(directory_path, "Directory does not exist");
    }

    // Collect all .vm files
    std::vector<std::string> vm_files;
    for (const auto& entry : fs::directory_iterator(directory_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vm") {
            vm_files.push_back(entry.path().string());
        }
    }

    // Sort alphabetically for consistent ordering
    std::sort(vm_files.begin(), vm_files.end());

    // Parse each file
    for (const auto& file : vm_files) {
        parse_file(file);
    }
}

// ==============================================================================
// Program Access
// ==============================================================================

VMProgram VMParser::get_program() const {
    VMProgram program;
    program.commands = commands_;
    program.label_positions = label_positions_;
    program.function_entry_points = function_entry_points_;
    program.source_files = source_files_;
    return program;
}

void VMParser::reset() {
    commands_.clear();
    label_positions_.clear();
    function_entry_points_.clear();
    source_files_.clear();
    current_file_.clear();
    current_function_.clear();
    current_line_ = 0;
}

// ==============================================================================
// Line Parsing
// ==============================================================================

std::optional<VMCommand> VMParser::parse_line(const std::string& line) {
    // Clean the line (remove comments and whitespace)
    std::string cleaned = clean_line(line);

    // Empty lines produce no command
    if (cleaned.empty()) {
        return std::nullopt;
    }

    // Tokenize the line
    std::vector<std::string> tokens = tokenize(cleaned);
    if (tokens.empty()) {
        return std::nullopt;
    }

    // Get the command keyword
    const std::string& keyword = tokens[0];

    // Parse based on command type
    // Arithmetic/logical commands (single word)
    if (keyword == "add")    return parse_arithmetic(keyword);
    if (keyword == "sub")    return parse_arithmetic(keyword);
    if (keyword == "neg")    return parse_arithmetic(keyword);
    if (keyword == "eq")     return parse_arithmetic(keyword);
    if (keyword == "gt")     return parse_arithmetic(keyword);
    if (keyword == "lt")     return parse_arithmetic(keyword);
    if (keyword == "and")    return parse_arithmetic(keyword);
    if (keyword == "or")     return parse_arithmetic(keyword);
    if (keyword == "not")    return parse_arithmetic(keyword);

    // Memory access commands
    if (keyword == "push")   return parse_push(tokens);
    if (keyword == "pop")    return parse_pop(tokens);

    // Program flow commands
    if (keyword == "label")   return parse_label(tokens);
    if (keyword == "goto")    return parse_goto(tokens);
    if (keyword == "if-goto") return parse_if_goto(tokens);

    // Function commands
    if (keyword == "function") return parse_function(tokens);
    if (keyword == "call")     return parse_call(tokens);
    if (keyword == "return")   return parse_return();

    // Unknown command - try to give helpful error
    // Check for common typos
    if (keyword == "pussh" || keyword == "psh") {
        error_with_suggestion("Unknown command", keyword, "push");
    }
    if (keyword == "popp" || keyword == "po") {
        error_with_suggestion("Unknown command", keyword, "pop");
    }
    if (keyword == "ad" || keyword == "addd") {
        error_with_suggestion("Unknown command", keyword, "add");
    }
    if (keyword == "substract" || keyword == "subtract") {
        error_with_suggestion("Unknown command", keyword, "sub");
    }
    if (keyword == "ifgoto" || keyword == "if_goto") {
        error_with_suggestion("Unknown command", keyword, "if-goto");
    }
    if (keyword == "func") {
        error_with_suggestion("Unknown command", keyword, "function");
    }
    if (keyword == "ret") {
        error_with_suggestion("Unknown command", keyword, "return");
    }

    error("Unknown command: '" + keyword + "'");
}

// ==============================================================================
// String Processing Helpers
// ==============================================================================

std::string VMParser::clean_line(const std::string& line) {
    std::string result = line;

    // Remove comment (everything after //)
    size_t comment_pos = result.find("//");
    if (comment_pos != std::string::npos) {
        result = result.substr(0, comment_pos);
    }

    // Trim leading whitespace
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";  // Line is all whitespace
    }

    // Trim trailing whitespace
    size_t end = result.find_last_not_of(" \t\r\n");

    return result.substr(start, end - start + 1);
}

std::vector<std::string> VMParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

// ==============================================================================
// Command Parsing
// ==============================================================================

ArithmeticCommand VMParser::parse_arithmetic(const std::string& op) {
    ArithmeticCommand cmd;
    cmd.source_line = current_line_;

    // Map string to operation enum
    if (op == "add") cmd.operation = ArithmeticOp::ADD;
    else if (op == "sub") cmd.operation = ArithmeticOp::SUB;
    else if (op == "neg") cmd.operation = ArithmeticOp::NEG;
    else if (op == "eq")  cmd.operation = ArithmeticOp::EQ;
    else if (op == "gt")  cmd.operation = ArithmeticOp::GT;
    else if (op == "lt")  cmd.operation = ArithmeticOp::LT;
    else if (op == "and") cmd.operation = ArithmeticOp::AND;
    else if (op == "or")  cmd.operation = ArithmeticOp::OR;
    else if (op == "not") cmd.operation = ArithmeticOp::NOT;
    else {
        error("Unknown arithmetic operation: '" + op + "'");
    }

    return cmd;
}

PushCommand VMParser::parse_push(const std::vector<std::string>& tokens) {
    // push segment index
    if (tokens.size() != 3) {
        error("push requires 2 arguments: push segment index");
    }

    PushCommand cmd;
    cmd.segment = parse_segment(tokens[1]);
    cmd.index = parse_index(tokens[2]);
    cmd.file_name = current_file_;
    cmd.source_line = current_line_;

    // Validate segment-specific constraints
    if (cmd.segment == SegmentType::TEMP && cmd.index > 7) {
        error("temp segment only has indices 0-7, got " + tokens[2]);
    }
    if (cmd.segment == SegmentType::POINTER && cmd.index > 1) {
        error("pointer segment only has indices 0-1, got " + tokens[2]);
    }

    return cmd;
}

PopCommand VMParser::parse_pop(const std::vector<std::string>& tokens) {
    // pop segment index
    if (tokens.size() != 3) {
        error("pop requires 2 arguments: pop segment index");
    }

    PopCommand cmd;
    cmd.segment = parse_segment(tokens[1]);
    cmd.index = parse_index(tokens[2]);
    cmd.file_name = current_file_;
    cmd.source_line = current_line_;

    // Cannot pop to constant segment
    if (cmd.segment == SegmentType::CONSTANT) {
        error("Cannot pop to constant segment (constants are read-only)");
    }

    // Validate segment-specific constraints
    if (cmd.segment == SegmentType::TEMP && cmd.index > 7) {
        error("temp segment only has indices 0-7, got " + tokens[2]);
    }
    if (cmd.segment == SegmentType::POINTER && cmd.index > 1) {
        error("pointer segment only has indices 0-1, got " + tokens[2]);
    }

    return cmd;
}

SegmentType VMParser::parse_segment(const std::string& segment_str) {
    if (segment_str == "local")    return SegmentType::LOCAL;
    if (segment_str == "argument") return SegmentType::ARGUMENT;
    if (segment_str == "this")     return SegmentType::THIS;
    if (segment_str == "that")     return SegmentType::THAT;
    if (segment_str == "constant") return SegmentType::CONSTANT;
    if (segment_str == "static")   return SegmentType::STATIC;
    if (segment_str == "temp")     return SegmentType::TEMP;
    if (segment_str == "pointer")  return SegmentType::POINTER;

    // Check for common typos
    if (segment_str == "loc" || segment_str == "lcl") {
        error_with_suggestion("Unknown segment", segment_str, "local");
    }
    if (segment_str == "arg" || segment_str == "args") {
        error_with_suggestion("Unknown segment", segment_str, "argument");
    }
    if (segment_str == "const") {
        error_with_suggestion("Unknown segment", segment_str, "constant");
    }
    if (segment_str == "tmp") {
        error_with_suggestion("Unknown segment", segment_str, "temp");
    }
    if (segment_str == "ptr") {
        error_with_suggestion("Unknown segment", segment_str, "pointer");
    }

    error("Unknown segment: '" + segment_str +
          "'. Valid segments: local, argument, this, that, constant, static, temp, pointer");
}

uint16_t VMParser::parse_index(const std::string& index_str) {
    // Check if string is a valid non-negative integer
    if (index_str.empty()) {
        error("Missing index");
    }

    for (char c : index_str) {
        if (!std::isdigit(c)) {
            error("Index must be a non-negative integer, got '" + index_str + "'");
        }
    }

    // Parse the integer
    try {
        unsigned long value = std::stoul(index_str);
        if (value > 32767) {
            error("Index out of range (max 32767), got " + index_str);
        }
        return static_cast<uint16_t>(value);
    } catch (const std::exception&) {
        error("Invalid index: '" + index_str + "'");
    }
}

LabelCommand VMParser::parse_label(const std::vector<std::string>& tokens) {
    // label labelName
    if (tokens.size() != 2) {
        error("label requires 1 argument: label labelName");
    }

    LabelCommand cmd;
    cmd.label_name = tokens[1];
    cmd.source_line = current_line_;

    if (!is_valid_label(cmd.label_name)) {
        error("Invalid label name: '" + cmd.label_name +
              "'. Labels must start with a letter, _, :, or . and contain only letters, digits, _, :, and .");
    }

    // Register the label position (scoped to current function)
    std::string scoped_name = make_scoped_label(cmd.label_name);
    if (label_positions_.count(scoped_name) > 0) {
        error("Duplicate label: '" + cmd.label_name + "' (already defined in this function)");
    }
    label_positions_[scoped_name] = commands_.size();

    return cmd;
}

GotoCommand VMParser::parse_goto(const std::vector<std::string>& tokens) {
    // goto labelName
    if (tokens.size() != 2) {
        error("goto requires 1 argument: goto labelName");
    }

    GotoCommand cmd;
    cmd.label_name = tokens[1];
    cmd.source_line = current_line_;

    return cmd;
}

IfGotoCommand VMParser::parse_if_goto(const std::vector<std::string>& tokens) {
    // if-goto labelName
    if (tokens.size() != 2) {
        error("if-goto requires 1 argument: if-goto labelName");
    }

    IfGotoCommand cmd;
    cmd.label_name = tokens[1];
    cmd.source_line = current_line_;

    return cmd;
}

FunctionCommand VMParser::parse_function(const std::vector<std::string>& tokens) {
    // function functionName nVars
    if (tokens.size() != 3) {
        error("function requires 2 arguments: function functionName nVars");
    }

    FunctionCommand cmd;
    cmd.function_name = tokens[1];
    cmd.num_locals = parse_index(tokens[2]);
    cmd.source_line = current_line_;

    if (!is_valid_identifier(cmd.function_name)) {
        error("Invalid function name: '" + cmd.function_name + "'");
    }

    // Update current function context (for label scoping)
    current_function_ = cmd.function_name;

    // Register function entry point
    if (function_entry_points_.count(cmd.function_name) > 0) {
        error("Duplicate function definition: '" + cmd.function_name + "'");
    }
    function_entry_points_[cmd.function_name] = commands_.size();

    return cmd;
}

CallCommand VMParser::parse_call(const std::vector<std::string>& tokens) {
    // call functionName nArgs
    if (tokens.size() != 3) {
        error("call requires 2 arguments: call functionName nArgs");
    }

    CallCommand cmd;
    cmd.function_name = tokens[1];
    cmd.num_args = parse_index(tokens[2]);
    cmd.source_line = current_line_;

    return cmd;
}

ReturnCommand VMParser::parse_return() {
    ReturnCommand cmd;
    cmd.source_line = current_line_;
    return cmd;
}

// ==============================================================================
// Helper Functions
// ==============================================================================

std::string VMParser::make_scoped_label(const std::string& label) const {
    // Labels are scoped to their containing function
    // Full name: functionName$labelName
    if (current_function_.empty()) {
        return label;  // No function context (shouldn't happen in valid code)
    }
    return current_function_ + "$" + label;
}

void VMParser::error(const std::string& message) {
    throw ParseError(current_file_, current_line_, message);
}

void VMParser::error_with_suggestion(const std::string& message,
                                      const std::string& wrong,
                                      const std::string& correct) {
    throw ParseError(current_file_, current_line_,
                     message + ": " + format_suggestion(wrong, correct));
}

// ==============================================================================
// Utility Functions
// ==============================================================================

bool is_valid_identifier(const std::string& str) {
    if (str.empty()) return false;

    // First character must be letter, underscore, or dot
    char first = str[0];
    if (!std::isalpha(first) && first != '_' && first != '.') {
        return false;
    }

    // Rest can be letters, digits, underscores, or dots
    for (size_t i = 1; i < str.length(); i++) {
        char c = str[i];
        if (!std::isalnum(c) && c != '_' && c != '.') {
            return false;
        }
    }

    return true;
}

bool is_valid_label(const std::string& str) {
    if (str.empty()) return false;

    // First character must be letter, underscore, colon, or dot
    char first = str[0];
    if (!std::isalpha(first) && first != '_' && first != ':' && first != '.') {
        return false;
    }

    // Rest can be letters, digits, underscores, colons, or dots
    for (size_t i = 1; i < str.length(); i++) {
        char c = str[i];
        if (!std::isalnum(c) && c != '_' && c != ':' && c != '.') {
            return false;
        }
    }

    return true;
}

std::string get_file_basename(const std::string& file_path) {
    namespace fs = std::filesystem;
    return fs::path(file_path).stem().string();
}

}  // namespace n2t
