// ==============================================================================
// Error Handling
// ==============================================================================
// This file defines error types and exception classes for the suite.
// Good error handling helps students understand what went wrong in their code.
// ==============================================================================

#ifndef NAND2TETRIS_COMMON_ERROR_HPP
#define NAND2TETRIS_COMMON_ERROR_HPP

#include <exception>
#include <string>
#include <sstream>
#include "types.hpp"

namespace n2t {

// ==============================================================================
// Error Categories
// ==============================================================================

/**
 * @brief Different categories of errors that can occur
 *
 * Categorizing errors helps users understand what kind of problem occurred:
 * - PARSE_ERROR: Couldn't understand the syntax of a file
 * - RUNTIME_ERROR: Error during execution (e.g., stack overflow, invalid memory access)
 * - LOGIC_ERROR: The logic is incorrect (e.g., chip produces wrong output)
 * - FILE_ERROR: Couldn't read or write a file
 * - INTERNAL_ERROR: Bug in the simulator itself (shouldn't happen!)
 */
enum class ErrorCategory {
    PARSE_ERROR,
    RUNTIME_ERROR,
    LOGIC_ERROR,
    FILE_ERROR,
    INTERNAL_ERROR
};

/**
 * @brief Convert ErrorCategory to string for display
 */
inline const char* error_category_to_string(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::PARSE_ERROR:    return "Parse Error";
        case ErrorCategory::RUNTIME_ERROR:  return "Runtime Error";
        case ErrorCategory::LOGIC_ERROR:    return "Logic Error";
        case ErrorCategory::FILE_ERROR:     return "File Error";
        case ErrorCategory::INTERNAL_ERROR: return "Internal Error";
        default:                            return "Unknown Error";
    }
}

// ==============================================================================
// Base Exception Class
// ==============================================================================

/**
 * @brief Base exception class for all Nand2Tetris suite errors
 *
 * This class provides rich context about errors:
 * - What category of error (parse, runtime, etc.)
 * - Where it occurred (file and line number)
 * - What went wrong (descriptive message)
 *
 * Example usage:
 *   throw N2TError(ErrorCategory::PARSE_ERROR, "Main.vm", 42,
 *                  "Invalid VM command: 'psh' (did you mean 'push'?)");
 *
 * This will produce:
 *   Parse Error in Main.vm:42 - Invalid VM command: 'psh' (did you mean 'push'?)
 */
class N2TError : public std::exception {
public:
    /**
     * @brief Construct an error with full context
     *
     * @param category What kind of error
     * @param file Which file the error is in
     * @param line Which line number (0 if unknown)
     * @param message Description of what went wrong
     */
    N2TError(ErrorCategory category,
             const std::string& file,
             LineNumber line,
             const std::string& message)
        : category_(category)
        , file_(file)
        , line_(line)
        , message_(message)
    {
        // Build the full error message with context
        std::ostringstream oss;
        oss << error_category_to_string(category);

        if (!file.empty()) {
            oss << " in " << file;
            if (line > 0) {
                oss << ":" << line;
            }
        }

        oss << " - " << message;
        full_message_ = oss.str();
    }

    /**
     * @brief Construct a simple error without file context
     *
     * @param category What kind of error
     * @param message Description of what went wrong
     */
    N2TError(ErrorCategory category, const std::string& message)
        : N2TError(category, "", 0, message)
    {}

    /**
     * @brief Get the full formatted error message
     *
     * This is what gets displayed to the user.
     */
    const char* what() const noexcept override {
        return full_message_.c_str();
    }

    /**
     * @brief Get the error category
     */
    ErrorCategory category() const { return category_; }

    /**
     * @brief Get the file where the error occurred
     */
    const std::string& file() const { return file_; }

    /**
     * @brief Get the line number where the error occurred (0 if unknown)
     */
    LineNumber line() const { return line_; }

    /**
     * @brief Get just the error message (without category/file/line)
     */
    const std::string& message() const { return message_; }

private:
    ErrorCategory category_;
    std::string file_;
    LineNumber line_;
    std::string message_;
    std::string full_message_;  // Cached formatted message
};

// ==============================================================================
// Specific Exception Types
// ==============================================================================
// These are convenience classes for common error types.
// They're more readable than N2TError(ErrorCategory::PARSE_ERROR, ...)
// ==============================================================================

/**
 * @brief Parse error - couldn't understand the syntax
 *
 * Examples:
 * - Invalid VM command
 * - Malformed assembly instruction
 * - Bad HDL syntax
 */
class ParseError : public N2TError {
public:
    ParseError(const std::string& file, LineNumber line, const std::string& message)
        : N2TError(ErrorCategory::PARSE_ERROR, file, line, message)
    {}

    ParseError(const std::string& message)
        : N2TError(ErrorCategory::PARSE_ERROR, message)
    {}
};

/**
 * @brief Runtime error - something went wrong during execution
 *
 * Examples:
 * - Stack overflow
 * - Stack underflow
 * - Invalid memory access
 * - Division by zero
 */
class RuntimeError : public N2TError {
public:
    RuntimeError(const std::string& file, LineNumber line, const std::string& message)
        : N2TError(ErrorCategory::RUNTIME_ERROR, file, line, message)
    {}

    RuntimeError(const std::string& message)
        : N2TError(ErrorCategory::RUNTIME_ERROR, message)
    {}
};

/**
 * @brief Logic error - the output is wrong
 *
 * Examples:
 * - Chip test failed (output doesn't match expected)
 * - Wrong computation result
 */
class LogicError : public N2TError {
public:
    LogicError(const std::string& file, LineNumber line, const std::string& message)
        : N2TError(ErrorCategory::LOGIC_ERROR, file, line, message)
    {}

    LogicError(const std::string& message)
        : N2TError(ErrorCategory::LOGIC_ERROR, message)
    {}
};

/**
 * @brief File error - couldn't read or write a file
 *
 * Examples:
 * - File not found
 * - Permission denied
 * - Disk full
 */
class FileError : public N2TError {
public:
    FileError(const std::string& file, const std::string& message)
        : N2TError(ErrorCategory::FILE_ERROR, file, 0, message)
    {}

    FileError(const std::string& message)
        : N2TError(ErrorCategory::FILE_ERROR, message)
    {}
};

/**
 * @brief Internal error - bug in the simulator itself
 *
 * These should never happen in a correct implementation.
 * If a user sees this, they should report it as a bug.
 *
 * Examples:
 * - Assertion failure
 * - Unexpected internal state
 * - Logic bug in simulator code
 */
class InternalError : public N2TError {
public:
    InternalError(const std::string& message)
        : N2TError(ErrorCategory::INTERNAL_ERROR, message)
    {}
};

// ==============================================================================
// Error Reporting Helpers
// ==============================================================================

/**
 * @brief Build a helpful error message with context
 *
 * This helper makes it easy to create detailed error messages.
 *
 * Example:
 *   throw ParseError(build_error_message(
 *       "Invalid command",
 *       actual_command,
 *       "Expected one of: push, pop, add, sub"
 *   ));
 *
 * Produces:
 *   "Invalid command: 'psh'. Expected one of: push, pop, add, sub"
 */
template<typename... Args>
std::string build_error_message(Args&&... args) {
    std::ostringstream oss;
    // Fold expression (C++17) - concatenate all arguments
    (oss << ... << args);
    return oss.str();
}

/**
 * @brief Format a suggestion for a typo
 *
 * When the user makes a typo, we can suggest the correct spelling.
 *
 * @param wrong What the user typed
 * @param correct What they probably meant
 * @return Formatted suggestion string
 *
 * Example:
 *   format_suggestion("psh", "push")
 * Returns:
 *   "'psh' (did you mean 'push'?)"
 */
inline std::string format_suggestion(const std::string& wrong, const std::string& correct) {
    return "'" + wrong + "' (did you mean '" + correct + "'?)";
}

}  // namespace n2t

#endif  // NAND2TETRIS_COMMON_ERROR_HPP
