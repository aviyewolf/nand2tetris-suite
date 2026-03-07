#include "line_editor.hpp"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
// No raw-mode support on Windows — always fall back to std::getline.
#define LINE_EDITOR_TTY 0
#else
#include <termios.h>
#include <unistd.h>
#define LINE_EDITOR_TTY 1
#endif

// ---------------------------------------------------------------------------
// RAII guard that puts the terminal into raw mode and restores it on scope exit
// ---------------------------------------------------------------------------
#if LINE_EDITOR_TTY
namespace {

class RawMode {
public:
    explicit RawMode(int fd) : fd_(fd), ok_(false) {
        if (::tcgetattr(fd_, &orig_) != 0) return;
        struct termios raw = orig_;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= CS8;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        if (::tcsetattr(fd_, TCSAFLUSH, &raw) == 0) ok_ = true;
    }
    ~RawMode() { if (ok_) ::tcsetattr(fd_, TCSAFLUSH, &orig_); }
    bool ok() const { return ok_; }
private:
    int fd_;
    bool ok_;
    struct termios orig_;
};

} // anonymous namespace
#endif // LINE_EDITOR_TTY

// ---------------------------------------------------------------------------
// LineEditor
// ---------------------------------------------------------------------------

LineEditor::LineEditor(const std::string& prompt) : prompt_(prompt) {}

void LineEditor::refresh_line(const std::string& buf, std::size_t cursor) const {
    // Carriage return, clear line, print prompt + buffer, reposition cursor
    std::string seq;
    seq += '\r';                           // move to column 0
    seq += "\x1b[K";                       // erase to end of line
    seq += prompt_;
    seq += buf;
    // Move cursor to the right position
    if (prompt_.size() + cursor < prompt_.size() + buf.size()) {
        // Move cursor left by (buf.size() - cursor)
        auto back = buf.size() - cursor;
        seq += "\x1b[" + std::to_string(back) + "D";
    }
    ::write(STDOUT_FILENO, seq.data(), seq.size());
}

std::optional<std::string> LineEditor::read_line() {
#if LINE_EDITOR_TTY
    if (!::isatty(STDIN_FILENO)) {
#endif
        // Non-TTY fallback
        std::cout << prompt_ << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return std::nullopt;
        return line;
#if LINE_EDITOR_TTY
    }

    RawMode raw(STDIN_FILENO);
    if (!raw.ok()) {
        // Couldn't enter raw mode — fall back
        std::cout << prompt_ << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return std::nullopt;
        return line;
    }

    std::string buf;
    std::size_t cursor = 0;
    std::size_t hist_index = history_.size(); // one past last = "new line"
    std::string saved_buf; // what the user typed before browsing history

    refresh_line(buf, cursor);

    while (true) {
        char c = 0;
        auto nread = ::read(STDIN_FILENO, &c, 1);
        if (nread <= 0) {
            // EOF
            std::cout << "\n";
            return std::nullopt;
        }

        switch (c) {
        case '\r': // Enter
        case '\n': {
            // Write a newline so the output starts on the next line.
            // Use raw write since we disabled OPOST.
            const char nl[] = "\r\n";
            ::write(STDOUT_FILENO, nl, 2);
            if (!buf.empty()) {
                // Avoid duplicate consecutive entries
                if (history_.empty() || history_.back() != buf)
                    history_.push_back(buf);
                if (history_.size() > max_history_)
                    history_.erase(history_.begin());
            }
            return buf;
        }

        case 127:   // Backspace (most terminals)
        case '\b': { // Ctrl-H
            if (cursor > 0) {
                buf.erase(cursor - 1, 1);
                --cursor;
                refresh_line(buf, cursor);
            }
            break;
        }

        case '\x1b': { // Escape sequence
            char seq[3];
            if (::read(STDIN_FILENO, &seq[0], 1) <= 0) break;
            if (::read(STDIN_FILENO, &seq[1], 1) <= 0) break;

            if (seq[0] == '[') {
                switch (seq[1]) {
                case 'A': // Up arrow
                    if (!history_.empty() && hist_index > 0) {
                        if (hist_index == history_.size())
                            saved_buf = buf; // save current input
                        --hist_index;
                        buf = history_[hist_index];
                        cursor = buf.size();
                        refresh_line(buf, cursor);
                    }
                    break;
                case 'B': // Down arrow
                    if (hist_index < history_.size()) {
                        ++hist_index;
                        if (hist_index == history_.size())
                            buf = saved_buf;
                        else
                            buf = history_[hist_index];
                        cursor = buf.size();
                        refresh_line(buf, cursor);
                    }
                    break;
                case 'C': // Right arrow
                    if (cursor < buf.size()) {
                        ++cursor;
                        refresh_line(buf, cursor);
                    }
                    break;
                case 'D': // Left arrow
                    if (cursor > 0) {
                        --cursor;
                        refresh_line(buf, cursor);
                    }
                    break;
                case 'H': // Home
                    cursor = 0;
                    refresh_line(buf, cursor);
                    break;
                case 'F': // End
                    cursor = buf.size();
                    refresh_line(buf, cursor);
                    break;
                case '3': { // Delete key: ESC [ 3 ~
                    char tilde = 0;
                    ::read(STDIN_FILENO, &tilde, 1);
                    if (tilde == '~' && cursor < buf.size()) {
                        buf.erase(cursor, 1);
                        refresh_line(buf, cursor);
                    }
                    break;
                }
                default:
                    break;
                }
            } else if (seq[0] == 'O') {
                // Some terminals send ESC O H / ESC O F for Home/End
                if (seq[1] == 'H') { cursor = 0; refresh_line(buf, cursor); }
                if (seq[1] == 'F') { cursor = buf.size(); refresh_line(buf, cursor); }
            }
            break;
        }

        case 1: // Ctrl-A — Home
            cursor = 0;
            refresh_line(buf, cursor);
            break;

        case 5: // Ctrl-E — End
            cursor = buf.size();
            refresh_line(buf, cursor);
            break;

        case 4: // Ctrl-D — EOF when line is empty
            if (buf.empty()) {
                const char nl[] = "\r\n";
                ::write(STDOUT_FILENO, nl, 2);
                return std::nullopt;
            }
            // Otherwise act like Delete
            if (cursor < buf.size()) {
                buf.erase(cursor, 1);
                refresh_line(buf, cursor);
            }
            break;

        case 12: // Ctrl-L — clear screen
            ::write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7);
            refresh_line(buf, cursor);
            break;

        default:
            if (c >= 32) { // printable
                buf.insert(cursor, 1, c);
                ++cursor;
                refresh_line(buf, cursor);
            }
            break;
        }
    }
#endif // LINE_EDITOR_TTY
}
