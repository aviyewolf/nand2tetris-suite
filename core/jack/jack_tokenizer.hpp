// ==============================================================================
// Jack Tokenizer
// ==============================================================================
// Tokenizes Jack source code into a stream of tokens. Follows the same pattern
// as the HDL parser (core/hdl/hdl_parser.hpp). Used by the declaration parser
// to extract class/function/variable declarations from .jack files.
// ==============================================================================

#ifndef NAND2TETRIS_JACK_TOKENIZER_HPP
#define NAND2TETRIS_JACK_TOKENIZER_HPP

#include "types.hpp"
#include <string>
#include <vector>

namespace n2t {

// ==============================================================================
// Token Types
// ==============================================================================

enum class JackTokenType {
    // Keywords
    CLASS, CONSTRUCTOR, FUNCTION, METHOD,
    FIELD, STATIC, VAR,
    INT, CHAR, BOOLEAN, VOID,
    TRUE, FALSE, NULL_CONST, THIS,
    LET, DO, IF, ELSE, WHILE, RETURN,

    // Literals
    INT_CONST,      // e.g. 42
    STRING_CONST,   // e.g. "hello"

    // Identifier
    IDENTIFIER,     // e.g. myVar, Point, Main

    // Symbols
    LBRACE,     // {
    RBRACE,     // }
    LPAREN,     // (
    RPAREN,     // )
    LBRACKET,   // [
    RBRACKET,   // ]
    DOT,        // .
    COMMA,      // ,
    SEMICOLON,  // ;
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    AMP,        // &
    PIPE,       // |
    LT,         // <
    GT,         // >
    EQ,         // =
    TILDE,      // ~

    END_OF_FILE
};

struct JackToken {
    JackTokenType type;
    std::string text;
    LineNumber line;
};

// ==============================================================================
// Jack Tokenizer
// ==============================================================================

class JackTokenizer {
public:
    // Tokenize source code into a vector of tokens
    static std::vector<JackToken> tokenize(const std::string& source,
                                           const std::string& filename = "<string>");

private:
    JackTokenizer(const std::string& source, const std::string& filename);

    std::vector<JackToken> run();
    void skip_whitespace_and_comments();
    JackToken read_token();
    JackToken read_string();
    JackToken read_number();
    JackToken read_identifier_or_keyword();

    const std::string& source_;
    const std::string& filename_;
    size_t pos_;
    LineNumber line_;

    char peek() const;
    char advance();
    bool at_end() const;
};

}  // namespace n2t

#endif  // NAND2TETRIS_JACK_TOKENIZER_HPP
