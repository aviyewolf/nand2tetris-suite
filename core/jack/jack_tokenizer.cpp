// ==============================================================================
// Jack Tokenizer - Implementation
// ==============================================================================

#include "jack_tokenizer.hpp"
#include "error.hpp"
#include <unordered_map>

namespace n2t {

// ==============================================================================
// Keyword table
// ==============================================================================

static const std::unordered_map<std::string, JackTokenType> KEYWORDS = {
    {"class",       JackTokenType::CLASS},
    {"constructor", JackTokenType::CONSTRUCTOR},
    {"function",    JackTokenType::FUNCTION},
    {"method",      JackTokenType::METHOD},
    {"field",       JackTokenType::FIELD},
    {"static",      JackTokenType::STATIC},
    {"var",         JackTokenType::VAR},
    {"int",         JackTokenType::INT},
    {"char",        JackTokenType::CHAR},
    {"boolean",     JackTokenType::BOOLEAN},
    {"void",        JackTokenType::VOID},
    {"true",        JackTokenType::TRUE},
    {"false",       JackTokenType::FALSE},
    {"null",        JackTokenType::NULL_CONST},
    {"this",        JackTokenType::THIS},
    {"let",         JackTokenType::LET},
    {"do",          JackTokenType::DO},
    {"if",          JackTokenType::IF},
    {"else",        JackTokenType::ELSE},
    {"while",       JackTokenType::WHILE},
    {"return",      JackTokenType::RETURN},
};

// ==============================================================================
// Public API
// ==============================================================================

std::vector<JackToken> JackTokenizer::tokenize(const std::string& source,
                                               const std::string& filename) {
    JackTokenizer tokenizer(source, filename);
    return tokenizer.run();
}

// ==============================================================================
// Constructor
// ==============================================================================

JackTokenizer::JackTokenizer(const std::string& source, const std::string& filename)
    : source_(source)
    , filename_(filename)
    , pos_(0)
    , line_(1)
{}

// ==============================================================================
// Tokenization
// ==============================================================================

std::vector<JackToken> JackTokenizer::run() {
    std::vector<JackToken> tokens;

    while (true) {
        skip_whitespace_and_comments();
        if (at_end()) break;
        tokens.push_back(read_token());
    }

    tokens.push_back({JackTokenType::END_OF_FILE, "", line_});
    return tokens;
}

char JackTokenizer::peek() const {
    if (at_end()) return '\0';
    return source_[pos_];
}

char JackTokenizer::advance() {
    char c = source_[pos_++];
    if (c == '\n') line_++;
    return c;
}

bool JackTokenizer::at_end() const {
    return pos_ >= source_.size();
}

void JackTokenizer::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }

        // Comments
        if (c == '/' && pos_ + 1 < source_.size()) {
            char next = source_[pos_ + 1];

            // Line comment: //
            if (next == '/') {
                pos_ += 2;
                while (!at_end() && peek() != '\n') pos_++;
                continue;
            }

            // Block comment: /* ... */
            if (next == '*') {
                LineNumber start_line = line_;
                pos_ += 2;
                while (!at_end()) {
                    if (peek() == '*' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
                        pos_ += 2;
                        break;
                    }
                    if (peek() == '\n') line_++;
                    pos_++;
                }
                if (at_end() && (source_.size() < 2 ||
                    source_[source_.size() - 2] != '*' || source_[source_.size() - 1] != '/')) {
                    throw ParseError(filename_, start_line, "Unterminated block comment");
                }
                continue;
            }
        }

        break;  // Not whitespace or comment
    }
}

JackToken JackTokenizer::read_token() {
    char c = peek();

    // String constant
    if (c == '"') return read_string();

    // Integer constant
    if (c >= '0' && c <= '9') return read_number();

    // Identifier or keyword
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        return read_identifier_or_keyword();
    }

    // Single-character symbols
    advance();
    JackTokenType type;
    switch (c) {
        case '{': type = JackTokenType::LBRACE;    break;
        case '}': type = JackTokenType::RBRACE;    break;
        case '(': type = JackTokenType::LPAREN;    break;
        case ')': type = JackTokenType::RPAREN;    break;
        case '[': type = JackTokenType::LBRACKET;  break;
        case ']': type = JackTokenType::RBRACKET;  break;
        case '.': type = JackTokenType::DOT;       break;
        case ',': type = JackTokenType::COMMA;     break;
        case ';': type = JackTokenType::SEMICOLON; break;
        case '+': type = JackTokenType::PLUS;      break;
        case '-': type = JackTokenType::MINUS;     break;
        case '*': type = JackTokenType::STAR;      break;
        case '/': type = JackTokenType::SLASH;     break;
        case '&': type = JackTokenType::AMP;       break;
        case '|': type = JackTokenType::PIPE;      break;
        case '<': type = JackTokenType::LT;        break;
        case '>': type = JackTokenType::GT;        break;
        case '=': type = JackTokenType::EQ;        break;
        case '~': type = JackTokenType::TILDE;     break;
        default:
            throw ParseError(filename_, line_,
                "Unexpected character: '" + std::string(1, c) + "'");
    }

    return {type, std::string(1, c), line_};
}

JackToken JackTokenizer::read_string() {
    LineNumber start_line = line_;
    advance();  // skip opening "

    std::string value;
    while (!at_end() && peek() != '"' && peek() != '\n') {
        value += advance();
    }

    if (at_end() || peek() != '"') {
        throw ParseError(filename_, start_line, "Unterminated string constant");
    }
    advance();  // skip closing "

    return {JackTokenType::STRING_CONST, value, start_line};
}

JackToken JackTokenizer::read_number() {
    LineNumber start_line = line_;
    std::string value;

    while (!at_end() && peek() >= '0' && peek() <= '9') {
        value += advance();
    }

    // Validate range: 0..32767
    long num = std::stol(value);
    if (num > 32767) {
        throw ParseError(filename_, start_line,
            "Integer constant out of range (0-32767): " + value);
    }

    return {JackTokenType::INT_CONST, value, start_line};
}

JackToken JackTokenizer::read_identifier_or_keyword() {
    LineNumber start_line = line_;
    std::string value;

    while (!at_end()) {
        char c = peek();
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') {
            value += advance();
        } else {
            break;
        }
    }

    // Check if it's a keyword
    auto it = KEYWORDS.find(value);
    if (it != KEYWORDS.end()) {
        return {it->second, value, start_line};
    }

    return {JackTokenType::IDENTIFIER, value, start_line};
}

}  // namespace n2t
