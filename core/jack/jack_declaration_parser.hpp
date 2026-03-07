// ==============================================================================
// Jack Declaration Parser
// ==============================================================================
// Parses Jack source files for declarations only: class structure, field/static
// variables, subroutine signatures, and local variables. Does NOT parse
// expressions or generate code — those are the hard parts students build in
// Projects 10-11.
//
// Used to auto-generate source maps for the Jack debugger, so students can
// debug .jack programs without needing their compiler to emit .smap files.
// ==============================================================================

#ifndef NAND2TETRIS_JACK_DECLARATION_PARSER_HPP
#define NAND2TETRIS_JACK_DECLARATION_PARSER_HPP

#include "jack_tokenizer.hpp"
#include "source_map.hpp"  // for JackVariable, JackVarKind
#include <string>
#include <vector>
#include <utility>

namespace n2t {

// ==============================================================================
// Parsed Declaration Types
// ==============================================================================

struct JackSubroutineInfo {
    std::string name;           // e.g. "main"
    std::string full_name;      // e.g. "Main.main"

    enum Kind { CONSTRUCTOR, METHOD, FUNCTION } kind;

    std::string return_type;
    std::vector<JackVariable> parameters;
    std::vector<JackVariable> locals;

    LineNumber decl_line;       // Line of the subroutine declaration

    // Statement start lines extracted by scanning the body at depth 1.
    // Each pair is (keyword, line_number), e.g. ("let", 5), ("do", 6).
    std::vector<std::pair<std::string, LineNumber>> statements;
};

struct JackClassInfo {
    std::string name;                       // e.g. "Main"
    std::string filename;                   // e.g. "Main.jack"
    std::vector<JackVariable> fields;
    std::vector<JackVariable> statics;
    std::vector<JackSubroutineInfo> subroutines;
};

// ==============================================================================
// Parser API
// ==============================================================================

// Parse a single .jack source file for declarations.
JackClassInfo parse_jack_class(const std::string& source,
                               const std::string& filename = "<string>");

}  // namespace n2t

#endif  // NAND2TETRIS_JACK_DECLARATION_PARSER_HPP
