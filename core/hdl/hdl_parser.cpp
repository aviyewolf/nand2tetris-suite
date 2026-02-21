// ==============================================================================
// HDL Parser Implementation
// ==============================================================================

#include "hdl_parser.hpp"
#include "error.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace n2t {

// ==============================================================================
// Public API
// ==============================================================================

HDLChipDef HDLParser::parse_string(const std::string& source,
                                    const std::string& filename) {
    filename_ = filename;
    tokenize(source);
    pos_ = 0;
    return parse_chip();
}

HDLChipDef HDLParser::parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw FileError(path, "Could not open HDL file");
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return parse_string(ss.str(), path);
}

// ==============================================================================
// Tokenizer
// ==============================================================================

void HDLParser::tokenize(const std::string& source) {
    tokens_.clear();
    src_ = source.c_str();
    src_len_ = source.size();
    src_pos_ = 0;
    current_line_ = 1;

    while (src_pos_ < src_len_) {
        skip_whitespace_and_comments();
        if (src_pos_ >= src_len_) break;

        Token tok = read_token();
        tokens_.push_back(tok);
    }

    Token eof;
    eof.type = TokenType::END_OF_FILE;
    eof.text = "";
    eof.line = current_line_;
    tokens_.push_back(eof);
}

void HDLParser::skip_whitespace_and_comments() {
    while (src_pos_ < src_len_) {
        char c = src_[src_pos_];

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r') {
            src_pos_++;
            continue;
        }
        if (c == '\n') {
            src_pos_++;
            current_line_++;
            continue;
        }

        // Line comment
        if (src_pos_ + 1 < src_len_ && c == '/' && src_[src_pos_ + 1] == '/') {
            src_pos_ += 2;
            while (src_pos_ < src_len_ && src_[src_pos_] != '\n') {
                src_pos_++;
            }
            continue;
        }

        // Block comment
        if (src_pos_ + 1 < src_len_ && c == '/' && src_[src_pos_ + 1] == '*') {
            src_pos_ += 2;
            while (src_pos_ + 1 < src_len_) {
                if (src_[src_pos_] == '\n') current_line_++;
                if (src_[src_pos_] == '*' && src_[src_pos_ + 1] == '/') {
                    src_pos_ += 2;
                    break;
                }
                src_pos_++;
            }
            continue;
        }

        break;
    }
}

bool HDLParser::is_identifier_char(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.';
}

HDLParser::Token HDLParser::read_token() {
    Token tok;
    tok.line = current_line_;
    char c = src_[src_pos_];

    // Single-character tokens
    auto single = [&](TokenType t) {
        tok.type = t;
        tok.text = std::string(1, c);
        src_pos_++;
        return tok;
    };

    switch (c) {
        case '{': return single(TokenType::LBRACE);
        case '}': return single(TokenType::RBRACE);
        case '(': return single(TokenType::LPAREN);
        case ')': return single(TokenType::RPAREN);
        case '[': return single(TokenType::LBRACKET);
        case ']': return single(TokenType::RBRACKET);
        case ',': return single(TokenType::COMMA);
        case ';': return single(TokenType::SEMICOLON);
        case ':': return single(TokenType::COLON);
        case '=': return single(TokenType::EQUALS);
        default: break;
    }

    // Dot or dotdot
    if (c == '.') {
        src_pos_++;
        if (src_pos_ < src_len_ && src_[src_pos_] == '.') {
            src_pos_++;
            tok.type = TokenType::DOTDOT;
            tok.text = "..";
            return tok;
        }
        tok.type = TokenType::DOT;
        tok.text = ".";
        return tok;
    }

    // Number
    if (c >= '0' && c <= '9') {
        size_t start = src_pos_;
        while (src_pos_ < src_len_ && src_[src_pos_] >= '0' && src_[src_pos_] <= '9') {
            src_pos_++;
        }
        tok.type = TokenType::NUMBER;
        tok.text = std::string(src_ + start, src_pos_ - start);
        return tok;
    }

    // Identifier or keyword
    if (is_identifier_char(c) && !(c >= '0' && c <= '9')) {
        size_t start = src_pos_;
        while (src_pos_ < src_len_ && is_identifier_char(src_[src_pos_])) {
            src_pos_++;
        }
        tok.text = std::string(src_ + start, src_pos_ - start);

        // Check keywords
        if (tok.text == "CHIP") tok.type = TokenType::KEYWORD_CHIP;
        else if (tok.text == "IN") tok.type = TokenType::KEYWORD_IN;
        else if (tok.text == "OUT") tok.type = TokenType::KEYWORD_OUT;
        else if (tok.text == "PARTS") tok.type = TokenType::KEYWORD_PARTS;
        else if (tok.text == "BUILTIN") tok.type = TokenType::KEYWORD_BUILTIN;
        else if (tok.text == "CLOCKED") tok.type = TokenType::KEYWORD_CLOCKED;
        else tok.type = TokenType::IDENTIFIER;

        return tok;
    }

    throw ParseError(filename_, current_line_,
                     std::string("Unexpected character: '") + c + "'");
}

// ==============================================================================
// Parser Helpers
// ==============================================================================

const HDLParser::Token& HDLParser::peek() const {
    return tokens_[pos_];
}

HDLParser::Token HDLParser::advance() {
    Token tok = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) pos_++;
    return tok;
}

HDLParser::Token HDLParser::expect(TokenType type, const std::string& context) {
    const Token& tok = peek();
    if (tok.type != type) {
        throw ParseError(filename_, tok.line,
                         "Expected " + context + ", got '" + tok.text + "'");
    }
    return advance();
}

bool HDLParser::match(TokenType type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

// ==============================================================================
// Recursive Descent Parsing
// ==============================================================================

HDLChipDef HDLParser::parse_chip() {
    HDLChipDef def;

    expect(TokenType::KEYWORD_CHIP, "'CHIP'");
    def.name = expect(TokenType::IDENTIFIER, "chip name").text;
    expect(TokenType::LBRACE, "'{'");

    // IN ports
    expect(TokenType::KEYWORD_IN, "'IN'");
    def.inputs = parse_port_list();
    expect(TokenType::SEMICOLON, "';' after IN ports");

    // OUT ports
    expect(TokenType::KEYWORD_OUT, "'OUT'");
    def.outputs = parse_port_list();
    expect(TokenType::SEMICOLON, "';' after OUT ports");

    // PARTS: or BUILTIN
    if (peek().type == TokenType::KEYWORD_PARTS) {
        advance();
        expect(TokenType::COLON, "':' after PARTS");
        def.parts = parse_parts();
        def.is_builtin = false;
    } else if (peek().type == TokenType::KEYWORD_BUILTIN) {
        advance();
        def.is_builtin = true;
        // Consume the builtin chip name identifier
        expect(TokenType::IDENTIFIER, "builtin chip name");
        expect(TokenType::SEMICOLON, "';' after BUILTIN");

        // Optional CLOCKED
        if (peek().type == TokenType::KEYWORD_CLOCKED) {
            advance();
            while (peek().type == TokenType::IDENTIFIER) {
                def.clocked_pins.push_back(advance().text);
                if (!match(TokenType::COMMA)) break;
            }
            expect(TokenType::SEMICOLON, "';' after CLOCKED");
        }
    } else {
        throw ParseError(filename_, peek().line,
                         "Expected 'PARTS:' or 'BUILTIN', got '" + peek().text + "'");
    }

    expect(TokenType::RBRACE, "'}'");
    return def;
}

std::vector<HDLPort> HDLParser::parse_port_list() {
    std::vector<HDLPort> ports;
    ports.push_back(parse_port());
    while (match(TokenType::COMMA)) {
        ports.push_back(parse_port());
    }
    return ports;
}

HDLPort HDLParser::parse_port() {
    HDLPort port;
    port.name = expect(TokenType::IDENTIFIER, "port name").text;
    port.width = 1;

    // Optional bus width: [N]
    if (match(TokenType::LBRACKET)) {
        std::string num = expect(TokenType::NUMBER, "bus width").text;
        port.width = static_cast<uint8_t>(std::stoi(num));
        expect(TokenType::RBRACKET, "']'");
    }

    return port;
}

std::vector<HDLPart> HDLParser::parse_parts() {
    std::vector<HDLPart> parts;
    while (peek().type == TokenType::IDENTIFIER) {
        parts.push_back(parse_part());
    }
    return parts;
}

HDLPart HDLParser::parse_part() {
    HDLPart part;
    part.source_line = peek().line;
    part.chip_name = expect(TokenType::IDENTIFIER, "part chip name").text;
    expect(TokenType::LPAREN, "'('");

    // Connection list
    part.connections.push_back(parse_connection());
    while (match(TokenType::COMMA)) {
        part.connections.push_back(parse_connection());
    }

    expect(TokenType::RPAREN, "')'");
    expect(TokenType::SEMICOLON, "';' after part");
    return part;
}

HDLConnection HDLParser::parse_connection() {
    HDLConnection conn;
    conn.internal = parse_pin_ref();
    expect(TokenType::EQUALS, "'='");
    conn.external = parse_pin_ref();
    return conn;
}

PinRef HDLParser::parse_pin_ref() {
    PinRef ref;
    ref.name = expect(TokenType::IDENTIFIER, "pin name").text;

    // Optional subscript: [i] or [i..j]
    if (match(TokenType::LBRACKET)) {
        std::string num = expect(TokenType::NUMBER, "bit index").text;
        ref.lo = std::stoi(num);
        if (match(TokenType::DOTDOT)) {
            std::string num2 = expect(TokenType::NUMBER, "bit index end").text;
            ref.hi = std::stoi(num2);
        } else {
            ref.hi = ref.lo;
        }
        expect(TokenType::RBRACKET, "']'");
    }

    return ref;
}

}  // namespace n2t
