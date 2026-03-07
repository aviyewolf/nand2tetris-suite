#ifndef LINE_EDITOR_HPP
#define LINE_EDITOR_HPP

#include <optional>
#include <string>
#include <vector>

/// Simple line editor with command history and cursor movement.
/// Uses POSIX termios for raw-mode input when stdin is a TTY;
/// falls back to std::getline for piped/redirected input.
class LineEditor {
public:
    explicit LineEditor(const std::string& prompt = "> ");

    /// Read one line of input.  Returns std::nullopt on EOF.
    std::optional<std::string> read_line();

private:
    std::string prompt_;
    std::vector<std::string> history_;
    static constexpr std::size_t max_history_ = 500;

    // Raw-mode helpers
    void refresh_line(const std::string& buf, std::size_t cursor) const;
};

#endif // LINE_EDITOR_HPP
