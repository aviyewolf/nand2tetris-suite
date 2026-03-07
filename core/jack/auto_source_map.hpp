// ==============================================================================
// Auto Source Map Generator
// ==============================================================================
// Generates a SourceMap from parsed Jack declarations and compiled VM commands.
// This lets students debug .jack programs without needing their compiler to
// emit .smap files.
// ==============================================================================

#ifndef NAND2TETRIS_AUTO_SOURCE_MAP_HPP
#define NAND2TETRIS_AUTO_SOURCE_MAP_HPP

#include "jack_declaration_parser.hpp"
#include "source_map.hpp"
#include "vm_command.hpp"
#include <vector>

namespace n2t {

// Generate a SourceMap by correlating parsed Jack class declarations with
// compiled VM commands. Populates symbol tables, class layouts, and
// line mappings using heuristic matching of VM statement patterns to
// Jack statement positions.
SourceMap generate_source_map(const std::vector<JackClassInfo>& classes,
                              const std::vector<VMCommand>& commands);

}  // namespace n2t

#endif  // NAND2TETRIS_AUTO_SOURCE_MAP_HPP
