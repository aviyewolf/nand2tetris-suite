// ==============================================================================
// HDL Parser
// ==============================================================================
// Parses HDL (Hardware Description Language) files into chip definition ASTs.
// Supports CHIP, IN, OUT, PARTS, BUILTIN, CLOCKED keywords.
// Handles bus subscripts, true/false constants, and // /* */ comments.
// ==============================================================================

#ifndef NAND2TETRIS_HDL_PARSER_HPP
#define NAND2TETRIS_HDL_PARSER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "types.hpp"

namespace n2t {

// ==============================================================================
// AST Types
// ==============================================================================

struct HDLPort {
    std::string name;
    uint8_t width = 1;  // 1 = single-bit, N = bus width
};

struct PinRef {
    std::string name;   // pin or wire name, or "true"/"false"
    int lo = -1;        // bit range start (-1 = full width)
    int hi = -1;        // bit range end
};

struct HDLConnection {
    PinRef internal;    // part's pin (left side of =)
    PinRef external;    // chip's pin/wire/constant (right side of =)
};

struct HDLPart {
    std::string chip_name;
    std::vector<HDLConnection> connections;
    LineNumber source_line = 0;
};

struct HDLChipDef {
    std::string name;
    std::vector<HDLPort> inputs;
    std::vector<HDLPort> outputs;
    std::vector<HDLPart> parts;         // empty if BUILTIN
    bool is_builtin = false;
    std::vector<std::string> clocked_pins;  // for Phase 2
};

// ==============================================================================
// Parser
// ==============================================================================

class HDLParser {
public:
    HDLChipDef parse_string(const std::string& source,
                            const std::string& filename = "<hdl>");
    HDLChipDef parse_file(const std::string& path);

private:
    // Token types
    enum class TokenType {
        IDENTIFIER, NUMBER, LBRACE, RBRACE, LPAREN, RPAREN,
        LBRACKET, RBRACKET, COMMA, SEMICOLON, COLON, EQUALS,
        DOTDOT, DOT, END_OF_FILE, KEYWORD_CHIP, KEYWORD_IN,
        KEYWORD_OUT, KEYWORD_PARTS, KEYWORD_BUILTIN, KEYWORD_CLOCKED
    };

    struct Token {
        TokenType type;
        std::string text;
        LineNumber line;
    };

    // Tokenizer
    void tokenize(const std::string& source);
    void skip_whitespace_and_comments();
    Token read_token();
    bool is_identifier_char(char c) const;

    // Parser helpers
    const Token& peek() const;
    Token advance();
    Token expect(TokenType type, const std::string& context);
    bool match(TokenType type);

    // Recursive descent parsing
    HDLChipDef parse_chip();
    std::vector<HDLPort> parse_port_list();
    HDLPort parse_port();
    std::vector<HDLPart> parse_parts();
    HDLPart parse_part();
    HDLConnection parse_connection();
    PinRef parse_pin_ref();

    // State
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::string filename_;
    const char* src_ = nullptr;
    size_t src_len_ = 0;
    size_t src_pos_ = 0;
    LineNumber current_line_ = 1;
};

}  // namespace n2t

#endif  // NAND2TETRIS_HDL_PARSER_HPP
