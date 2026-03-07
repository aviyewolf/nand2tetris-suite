// ==============================================================================
// Jack Declaration Parser - Implementation
// ==============================================================================

#include "jack_declaration_parser.hpp"
#include "error.hpp"
#include <algorithm>

namespace n2t {

// ==============================================================================
// Internal parser class
// ==============================================================================

class JackDeclParser {
public:
    JackDeclParser(const std::vector<JackToken>& tokens, const std::string& filename)
        : tokens_(tokens), filename_(filename), pos_(0) {}

    JackClassInfo parse();

private:
    const std::vector<JackToken>& tokens_;
    const std::string& filename_;
    size_t pos_;

    // Token access
    const JackToken& peek() const { return tokens_[pos_]; }

    const JackToken& advance() {
        const auto& t = tokens_[pos_];
        if (pos_ + 1 < tokens_.size()) pos_++;
        return t;
    }

    bool match(JackTokenType type) {
        if (peek().type == type) {
            advance();
            return true;
        }
        return false;
    }

    const JackToken& expect(JackTokenType type, const std::string& context) {
        if (peek().type != type) {
            throw ParseError(filename_, peek().line,
                "Expected " + context + ", got '" + peek().text + "'");
        }
        return advance();
    }

    bool is_type_keyword() const {
        auto t = peek().type;
        return t == JackTokenType::INT || t == JackTokenType::CHAR ||
               t == JackTokenType::BOOLEAN || t == JackTokenType::VOID;
    }

    bool is_subroutine_keyword() const {
        auto t = peek().type;
        return t == JackTokenType::CONSTRUCTOR || t == JackTokenType::FUNCTION ||
               t == JackTokenType::METHOD;
    }

    // Parsing methods
    std::string parse_type();
    void parse_class_var_dec(JackClassInfo& info);
    JackSubroutineInfo parse_subroutine(const std::string& class_name);
    void parse_parameter_list(JackSubroutineInfo& sub);
    void parse_local_vars(JackSubroutineInfo& sub);
    void scan_subroutine_body(JackSubroutineInfo& sub);
};

// ==============================================================================
// Public API
// ==============================================================================

JackClassInfo parse_jack_class(const std::string& source,
                               const std::string& filename) {
    auto tokens = JackTokenizer::tokenize(source, filename);
    JackDeclParser parser(tokens, filename);
    return parser.parse();
}

// ==============================================================================
// Top-level class parsing
// ==============================================================================

JackClassInfo JackDeclParser::parse() {
    JackClassInfo info;
    info.filename = filename_;

    // class ClassName {
    expect(JackTokenType::CLASS, "'class'");
    info.name = expect(JackTokenType::IDENTIFIER, "class name").text;
    expect(JackTokenType::LBRACE, "'{'");

    // Class body: field/static declarations, then subroutines
    while (peek().type != JackTokenType::RBRACE &&
           peek().type != JackTokenType::END_OF_FILE) {

        if (peek().type == JackTokenType::FIELD ||
            peek().type == JackTokenType::STATIC) {
            parse_class_var_dec(info);
        } else if (is_subroutine_keyword()) {
            info.subroutines.push_back(parse_subroutine(info.name));
        } else {
            throw ParseError(filename_, peek().line,
                "Expected field, static, or subroutine declaration, got '"
                + peek().text + "'");
        }
    }

    expect(JackTokenType::RBRACE, "'}'");
    return info;
}

// ==============================================================================
// Type parsing
// ==============================================================================

std::string JackDeclParser::parse_type() {
    if (is_type_keyword() || peek().type == JackTokenType::IDENTIFIER) {
        return advance().text;
    }
    throw ParseError(filename_, peek().line,
        "Expected type name, got '" + peek().text + "'");
}

// ==============================================================================
// Class variable declarations: field/static type name (, name)* ;
// ==============================================================================

void JackDeclParser::parse_class_var_dec(JackClassInfo& info) {
    JackVarKind kind;
    if (peek().type == JackTokenType::FIELD) {
        kind = JackVarKind::FIELD;
    } else {
        kind = JackVarKind::STATIC;
    }
    advance();

    std::string type = parse_type();

    // Determine which vector and starting index
    auto& vars = (kind == JackVarKind::FIELD) ? info.fields : info.statics;

    // First variable name
    std::string name = expect(JackTokenType::IDENTIFIER, "variable name").text;
    vars.push_back({name, type, kind, static_cast<uint16_t>(vars.size())});

    // Additional names separated by commas
    while (match(JackTokenType::COMMA)) {
        name = expect(JackTokenType::IDENTIFIER, "variable name").text;
        vars.push_back({name, type, kind, static_cast<uint16_t>(vars.size())});
    }

    expect(JackTokenType::SEMICOLON, "';'");
}

// ==============================================================================
// Subroutine parsing
// ==============================================================================

JackSubroutineInfo JackDeclParser::parse_subroutine(const std::string& class_name) {
    JackSubroutineInfo sub;
    sub.decl_line = peek().line;

    // Kind: constructor | function | method
    if (peek().type == JackTokenType::CONSTRUCTOR) {
        sub.kind = JackSubroutineInfo::CONSTRUCTOR;
    } else if (peek().type == JackTokenType::FUNCTION) {
        sub.kind = JackSubroutineInfo::FUNCTION;
    } else {
        sub.kind = JackSubroutineInfo::METHOD;
    }
    advance();

    // Return type (void | type)
    sub.return_type = parse_type();

    // Subroutine name
    sub.name = expect(JackTokenType::IDENTIFIER, "subroutine name").text;
    sub.full_name = class_name + "." + sub.name;

    // Parameter list
    expect(JackTokenType::LPAREN, "'('");
    parse_parameter_list(sub);
    expect(JackTokenType::RPAREN, "')'");

    // Subroutine body: { varDec* statements }
    expect(JackTokenType::LBRACE, "'{'");

    // Parse local variable declarations
    while (peek().type == JackTokenType::VAR) {
        parse_local_vars(sub);
    }

    // Scan statement body for statement keywords at brace depth 0
    // (we're already inside the subroutine's opening brace)
    scan_subroutine_body(sub);

    return sub;
}

// ==============================================================================
// Parameter list: ( (type name (, type name)*)? )
// ==============================================================================

void JackDeclParser::parse_parameter_list(JackSubroutineInfo& sub) {
    // For methods, argument 0 is 'this' (implicit)
    uint16_t arg_index = (sub.kind == JackSubroutineInfo::METHOD) ? 1 : 0;

    if (peek().type == JackTokenType::RPAREN) return;  // empty parameter list

    // First parameter
    std::string type = parse_type();
    std::string name = expect(JackTokenType::IDENTIFIER, "parameter name").text;
    sub.parameters.push_back({name, type, JackVarKind::ARGUMENT, arg_index++});

    // Additional parameters
    while (match(JackTokenType::COMMA)) {
        type = parse_type();
        name = expect(JackTokenType::IDENTIFIER, "parameter name").text;
        sub.parameters.push_back({name, type, JackVarKind::ARGUMENT, arg_index++});
    }
}

// ==============================================================================
// Local variable declarations: var type name (, name)* ;
// ==============================================================================

void JackDeclParser::parse_local_vars(JackSubroutineInfo& sub) {
    expect(JackTokenType::VAR, "'var'");
    std::string type = parse_type();

    std::string name = expect(JackTokenType::IDENTIFIER, "variable name").text;
    sub.locals.push_back({name, type, JackVarKind::LOCAL,
                          static_cast<uint16_t>(sub.locals.size())});

    while (match(JackTokenType::COMMA)) {
        name = expect(JackTokenType::IDENTIFIER, "variable name").text;
        sub.locals.push_back({name, type, JackVarKind::LOCAL,
                              static_cast<uint16_t>(sub.locals.size())});
    }

    expect(JackTokenType::SEMICOLON, "';'");
}

// ==============================================================================
// Scan subroutine body for statement keywords
// ==============================================================================
// After var declarations, scan tokens counting { } depth. At depth 0 (relative
// to the body), record let/do/if/while/return keywords with their line numbers.
// This gives us statement start lines without parsing expressions.
// We stop when we reach the closing } of the subroutine body (depth -1).

void JackDeclParser::scan_subroutine_body(JackSubroutineInfo& sub) {
    int depth = 0;

    while (peek().type != JackTokenType::END_OF_FILE) {
        const auto& tok = peek();

        if (tok.type == JackTokenType::LBRACE) {
            depth++;
            advance();
            continue;
        }

        if (tok.type == JackTokenType::RBRACE) {
            if (depth == 0) {
                // This is the closing brace of the subroutine body
                advance();
                return;
            }
            depth--;
            advance();
            continue;
        }

        // Record statement-starting keywords at depth 0
        if (depth == 0) {
            if (tok.type == JackTokenType::LET ||
                tok.type == JackTokenType::DO ||
                tok.type == JackTokenType::IF ||
                tok.type == JackTokenType::WHILE ||
                tok.type == JackTokenType::RETURN) {
                sub.statements.push_back({tok.text, tok.line});
            }
        }

        advance();
    }

    throw ParseError(filename_, peek().line, "Unterminated subroutine body");
}

}  // namespace n2t
