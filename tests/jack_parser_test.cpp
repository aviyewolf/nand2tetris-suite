// ==============================================================================
// Jack Parser Tests
// ==============================================================================
// Tests for Jack tokenizer, declaration parser, and auto source map generator.
// Uses assertions rather than a test framework for consistency.
// ==============================================================================

#include "jack_tokenizer.hpp"
#include "jack_declaration_parser.hpp"
#include "auto_source_map.hpp"
#include "jack_debugger.hpp"
#include "vm_parser.hpp"
#include <iostream>
#include <cassert>

using namespace n2t;

static int test_count = 0;
static int pass_count = 0;
static bool had_failure = false;

static void check(bool condition, const std::string& name) {
    test_count++;
    if (condition) {
        pass_count++;
        std::cout << "PASS: " << name << "\n";
    } else {
        std::cout << "FAIL: " << name << "\n";
        had_failure = true;
    }
}

// ==============================================================================
// Tokenizer Tests
// ==============================================================================

static void test_tokenizer_keywords() {
    std::cout << "\n--- Tokenizer: Keywords ---\n";

    auto tokens = JackTokenizer::tokenize("class Main { }");
    check(tokens.size() == 5, "4 tokens + EOF");
    check(tokens[0].type == JackTokenType::CLASS, "first token is CLASS");
    check(tokens[1].type == JackTokenType::IDENTIFIER, "second token is IDENTIFIER");
    check(tokens[1].text == "Main", "identifier text is Main");
    check(tokens[2].type == JackTokenType::LBRACE, "third token is LBRACE");
    check(tokens[3].type == JackTokenType::RBRACE, "fourth token is RBRACE");
    check(tokens[4].type == JackTokenType::END_OF_FILE, "last token is EOF");
}

static void test_tokenizer_all_keywords() {
    std::cout << "\n--- Tokenizer: All Keywords ---\n";

    auto tokens = JackTokenizer::tokenize(
        "class constructor function method field static var "
        "int char boolean void true false null this "
        "let do if else while return"
    );

    check(tokens[0].type == JackTokenType::CLASS, "class keyword");
    check(tokens[1].type == JackTokenType::CONSTRUCTOR, "constructor keyword");
    check(tokens[2].type == JackTokenType::FUNCTION, "function keyword");
    check(tokens[3].type == JackTokenType::METHOD, "method keyword");
    check(tokens[4].type == JackTokenType::FIELD, "field keyword");
    check(tokens[5].type == JackTokenType::STATIC, "static keyword");
    check(tokens[6].type == JackTokenType::VAR, "var keyword");
    check(tokens[7].type == JackTokenType::INT, "int keyword");
    check(tokens[8].type == JackTokenType::CHAR, "char keyword");
    check(tokens[9].type == JackTokenType::BOOLEAN, "boolean keyword");
    check(tokens[10].type == JackTokenType::VOID, "void keyword");
    check(tokens[11].type == JackTokenType::TRUE, "true keyword");
    check(tokens[12].type == JackTokenType::FALSE, "false keyword");
    check(tokens[13].type == JackTokenType::NULL_CONST, "null keyword");
    check(tokens[14].type == JackTokenType::THIS, "this keyword");
    check(tokens[15].type == JackTokenType::LET, "let keyword");
    check(tokens[16].type == JackTokenType::DO, "do keyword");
    check(tokens[17].type == JackTokenType::IF, "if keyword");
    check(tokens[18].type == JackTokenType::ELSE, "else keyword");
    check(tokens[19].type == JackTokenType::WHILE, "while keyword");
    check(tokens[20].type == JackTokenType::RETURN, "return keyword");
}

static void test_tokenizer_literals() {
    std::cout << "\n--- Tokenizer: Literals ---\n";

    auto tokens = JackTokenizer::tokenize("42 \"hello world\" 0 32767");
    check(tokens[0].type == JackTokenType::INT_CONST, "first is INT_CONST");
    check(tokens[0].text == "42", "int value is 42");
    check(tokens[1].type == JackTokenType::STRING_CONST, "second is STRING_CONST");
    check(tokens[1].text == "hello world", "string value is hello world");
    check(tokens[2].type == JackTokenType::INT_CONST, "third is INT_CONST");
    check(tokens[2].text == "0", "int value is 0");
    check(tokens[3].type == JackTokenType::INT_CONST, "fourth is INT_CONST");
    check(tokens[3].text == "32767", "int value is 32767");
}

static void test_tokenizer_symbols() {
    std::cout << "\n--- Tokenizer: Symbols ---\n";

    auto tokens = JackTokenizer::tokenize("{}()[].,;+-*/&|<>=~");
    check(tokens[0].type == JackTokenType::LBRACE, "{ symbol");
    check(tokens[1].type == JackTokenType::RBRACE, "} symbol");
    check(tokens[2].type == JackTokenType::LPAREN, "( symbol");
    check(tokens[3].type == JackTokenType::RPAREN, ") symbol");
    check(tokens[4].type == JackTokenType::LBRACKET, "[ symbol");
    check(tokens[5].type == JackTokenType::RBRACKET, "] symbol");
    check(tokens[6].type == JackTokenType::DOT, ". symbol");
    check(tokens[7].type == JackTokenType::COMMA, ", symbol");
    check(tokens[8].type == JackTokenType::SEMICOLON, "; symbol");
    check(tokens[9].type == JackTokenType::PLUS, "+ symbol");
    check(tokens[10].type == JackTokenType::MINUS, "- symbol");
    check(tokens[11].type == JackTokenType::STAR, "* symbol");
    check(tokens[12].type == JackTokenType::SLASH, "/ symbol");
    check(tokens[13].type == JackTokenType::AMP, "& symbol");
    check(tokens[14].type == JackTokenType::PIPE, "| symbol");
    check(tokens[15].type == JackTokenType::LT, "< symbol");
    check(tokens[16].type == JackTokenType::GT, "> symbol");
    check(tokens[17].type == JackTokenType::EQ, "= symbol");
    check(tokens[18].type == JackTokenType::TILDE, "~ symbol");
}

static void test_tokenizer_comments() {
    std::cout << "\n--- Tokenizer: Comments ---\n";

    auto tokens = JackTokenizer::tokenize(
        "// line comment\n"
        "class /* block comment */ Main { }"
    );
    check(tokens[0].type == JackTokenType::CLASS, "class after comments");
    check(tokens[1].type == JackTokenType::IDENTIFIER, "Main after block comment");
    check(tokens[1].text == "Main", "identifier is Main");
}

static void test_tokenizer_line_numbers() {
    std::cout << "\n--- Tokenizer: Line Numbers ---\n";

    auto tokens = JackTokenizer::tokenize(
        "class\n"
        "Main\n"
        "{\n"
        "}"
    );
    check(tokens[0].line == 1, "class on line 1");
    check(tokens[1].line == 2, "Main on line 2");
    check(tokens[2].line == 3, "{ on line 3");
    check(tokens[3].line == 4, "} on line 4");
}

// ==============================================================================
// Declaration Parser Tests
// ==============================================================================

static void test_parser_simple_class() {
    std::cout << "\n--- Parser: Simple Class ---\n";

    auto info = parse_jack_class(
        "class Main {\n"
        "  function void main() {\n"
        "    return;\n"
        "  }\n"
        "}\n",
        "Main.jack"
    );

    check(info.name == "Main", "class name is Main");
    check(info.filename == "Main.jack", "filename is Main.jack");
    check(info.fields.empty(), "no fields");
    check(info.statics.empty(), "no statics");
    check(info.subroutines.size() == 1, "1 subroutine");
    check(info.subroutines[0].name == "main", "subroutine name is main");
    check(info.subroutines[0].full_name == "Main.main", "full name is Main.main");
    check(info.subroutines[0].kind == JackSubroutineInfo::FUNCTION, "kind is FUNCTION");
    check(info.subroutines[0].return_type == "void", "return type is void");
    check(info.subroutines[0].parameters.empty(), "no parameters");
    check(info.subroutines[0].locals.empty(), "no locals");
}

static void test_parser_fields_and_statics() {
    std::cout << "\n--- Parser: Fields and Statics ---\n";

    auto info = parse_jack_class(
        "class Point {\n"
        "  field int x, y;\n"
        "  static int count;\n"
        "  constructor Point new(int ax, int ay) {\n"
        "    let x = ax;\n"
        "    let y = ay;\n"
        "    let count = count + 1;\n"
        "    return this;\n"
        "  }\n"
        "}\n",
        "Point.jack"
    );

    check(info.name == "Point", "class name is Point");
    check(info.fields.size() == 2, "2 fields");
    check(info.fields[0].name == "x", "first field is x");
    check(info.fields[0].type_name == "int", "x type is int");
    check(info.fields[0].kind == JackVarKind::FIELD, "x kind is FIELD");
    check(info.fields[0].index == 0, "x index is 0");
    check(info.fields[1].name == "y", "second field is y");
    check(info.fields[1].index == 1, "y index is 1");

    check(info.statics.size() == 1, "1 static");
    check(info.statics[0].name == "count", "static is count");
    check(info.statics[0].kind == JackVarKind::STATIC, "count kind is STATIC");
    check(info.statics[0].index == 0, "count index is 0");

    check(info.subroutines.size() == 1, "1 subroutine");
    auto& sub = info.subroutines[0];
    check(sub.kind == JackSubroutineInfo::CONSTRUCTOR, "kind is CONSTRUCTOR");
    check(sub.return_type == "Point", "return type is Point");
    check(sub.name == "new", "name is new");
}

static void test_parser_parameters() {
    std::cout << "\n--- Parser: Parameters ---\n";

    auto info = parse_jack_class(
        "class Math {\n"
        "  function int multiply(int x, int y) {\n"
        "    var int sum;\n"
        "    let sum = 0;\n"
        "    return sum;\n"
        "  }\n"
        "}\n",
        "Math.jack"
    );

    auto& sub = info.subroutines[0];
    check(sub.parameters.size() == 2, "2 parameters");
    check(sub.parameters[0].name == "x", "first param is x");
    check(sub.parameters[0].type_name == "int", "x type is int");
    check(sub.parameters[0].kind == JackVarKind::ARGUMENT, "x kind is ARGUMENT");
    check(sub.parameters[0].index == 0, "x index is 0 (function, not method)");
    check(sub.parameters[1].name == "y", "second param is y");
    check(sub.parameters[1].index == 1, "y index is 1");

    check(sub.locals.size() == 1, "1 local");
    check(sub.locals[0].name == "sum", "local is sum");
    check(sub.locals[0].kind == JackVarKind::LOCAL, "sum kind is LOCAL");
    check(sub.locals[0].index == 0, "sum index is 0");
}

static void test_parser_method_arg_shift() {
    std::cout << "\n--- Parser: Method Argument Shift ---\n";

    auto info = parse_jack_class(
        "class Point {\n"
        "  field int x;\n"
        "  method int distance(Point other) {\n"
        "    return 0;\n"
        "  }\n"
        "}\n",
        "Point.jack"
    );

    auto& sub = info.subroutines[0];
    check(sub.kind == JackSubroutineInfo::METHOD, "kind is METHOD");
    // For methods, arg 0 is 'this' (implicit), so 'other' should be arg 1
    check(sub.parameters.size() == 1, "1 explicit parameter");
    check(sub.parameters[0].name == "other", "param is other");
    check(sub.parameters[0].index == 1, "other index is 1 (shifted for method)");
}

static void test_parser_statement_scanning() {
    std::cout << "\n--- Parser: Statement Scanning ---\n";

    auto info = parse_jack_class(
        "class Main {\n"                      // line 1
        "  function void main() {\n"          // line 2
        "    var int x, y;\n"                 // line 3
        "    let x = 10;\n"                   // line 4
        "    let y = 20;\n"                   // line 5
        "    do Output.printInt(x + y);\n"    // line 6
        "    if (x > 0) {\n"                  // line 7
        "      do Output.printString(\"pos\");\n"  // line 8 (inside if, depth > 0)
        "    }\n"
        "    while (y > 0) {\n"               // line 10
        "      let y = y - 1;\n"              // line 11 (inside while, depth > 0)
        "    }\n"
        "    return;\n"                       // line 13
        "  }\n"
        "}\n",
        "Main.jack"
    );

    auto& sub = info.subroutines[0];
    check(sub.statements.size() == 6, "6 top-level statements (let, let, do, if, while, return)");
    if (sub.statements.size() >= 6) {
        check(sub.statements[0].first == "let", "stmt 0 is let");
        check(sub.statements[0].second == 4, "let on line 4");
        check(sub.statements[1].first == "let", "stmt 1 is let");
        check(sub.statements[1].second == 5, "let on line 5");
        check(sub.statements[2].first == "do", "stmt 2 is do");
        check(sub.statements[2].second == 6, "do on line 6");
        check(sub.statements[3].first == "if", "stmt 3 is if");
        check(sub.statements[3].second == 7, "if on line 7");
        check(sub.statements[4].first == "while", "stmt 4 is while");
        check(sub.statements[5].first == "return", "stmt 5 is return");
    }
}

static void test_parser_multiple_var_decls() {
    std::cout << "\n--- Parser: Multiple Var Declarations ---\n";

    auto info = parse_jack_class(
        "class Test {\n"
        "  function void foo() {\n"
        "    var int a;\n"
        "    var String b, c;\n"
        "    var boolean d;\n"
        "    return;\n"
        "  }\n"
        "}\n",
        "Test.jack"
    );

    auto& sub = info.subroutines[0];
    check(sub.locals.size() == 4, "4 locals");
    check(sub.locals[0].name == "a", "local 0 is a");
    check(sub.locals[0].index == 0, "a index 0");
    check(sub.locals[1].name == "b", "local 1 is b");
    check(sub.locals[1].index == 1, "b index 1");
    check(sub.locals[1].type_name == "String", "b type is String");
    check(sub.locals[2].name == "c", "local 2 is c");
    check(sub.locals[2].index == 2, "c index 2");
    check(sub.locals[3].name == "d", "local 3 is d");
    check(sub.locals[3].index == 3, "d index 3");
}

static void test_parser_multiple_subroutines() {
    std::cout << "\n--- Parser: Multiple Subroutines ---\n";

    auto info = parse_jack_class(
        "class Foo {\n"
        "  function void bar() {\n"
        "    return;\n"
        "  }\n"
        "  method int baz(int x) {\n"
        "    return x;\n"
        "  }\n"
        "  constructor Foo new() {\n"
        "    return this;\n"
        "  }\n"
        "}\n",
        "Foo.jack"
    );

    check(info.subroutines.size() == 3, "3 subroutines");
    check(info.subroutines[0].full_name == "Foo.bar", "first is Foo.bar");
    check(info.subroutines[0].kind == JackSubroutineInfo::FUNCTION, "bar is FUNCTION");
    check(info.subroutines[1].full_name == "Foo.baz", "second is Foo.baz");
    check(info.subroutines[1].kind == JackSubroutineInfo::METHOD, "baz is METHOD");
    check(info.subroutines[2].full_name == "Foo.new", "third is Foo.new");
    check(info.subroutines[2].kind == JackSubroutineInfo::CONSTRUCTOR, "new is CONSTRUCTOR");
}

// ==============================================================================
// Auto Source Map Tests
// ==============================================================================

static void test_auto_source_map_basic() {
    std::cout << "\n--- Auto Source Map: Basic ---\n";

    // A simple Jack class
    auto info = parse_jack_class(
        "class Main {\n"                      // line 1
        "  function void main() {\n"          // line 2
        "    var int x;\n"                    // line 3
        "    let x = 42;\n"                   // line 4
        "    return;\n"                       // line 5
        "  }\n"
        "}\n",
        "Main.jack"
    );

    // Corresponding VM code
    std::string vm_source =
        "function Main.main 1\n"
        "push constant 42\n"
        "pop local 0\n"
        "push constant 0\n"
        "return\n";

    VMParser parser;
    parser.parse_string(vm_source, "Main.vm");
    auto program = parser.get_program();

    std::vector<JackClassInfo> classes = {info};
    SourceMap smap = generate_source_map(classes, program.commands);

    check(!smap.empty(), "source map is not empty");

    // Check that we have function symbols
    auto* symbols = smap.get_function_symbols("Main.main");
    check(symbols != nullptr, "Main.main symbols found");
    if (symbols) {
        check(symbols->locals.size() == 1, "1 local variable");
        check(symbols->locals[0].name == "x", "local is x");
        check(symbols->locals[0].type_name == "int", "x type is int");
    }

    // Check that there are entries mapping to the function
    auto entry = smap.get_entry_for_vm(0);
    check(entry.has_value(), "VM index 0 has mapping");
    if (entry) {
        check(entry->function_name == "Main.main", "maps to Main.main");
    }
}

static void test_auto_source_map_with_fields() {
    std::cout << "\n--- Auto Source Map: Class with Fields ---\n";

    auto info = parse_jack_class(
        "class Point {\n"
        "  field int x, y;\n"
        "  constructor Point new(int ax, int ay) {\n"
        "    let x = ax;\n"
        "    let y = ay;\n"
        "    return this;\n"
        "  }\n"
        "}\n",
        "Point.jack"
    );

    std::string vm_source =
        "function Point.new 0\n"
        "push constant 2\n"
        "call Memory.alloc 1\n"
        "pop pointer 0\n"
        "push argument 0\n"
        "pop this 0\n"
        "push argument 1\n"
        "pop this 1\n"
        "push pointer 0\n"
        "return\n";

    VMParser parser;
    parser.parse_string(vm_source, "Point.vm");
    auto program = parser.get_program();

    std::vector<JackClassInfo> classes = {info};
    SourceMap smap = generate_source_map(classes, program.commands);

    // Check class layout
    auto* layout = smap.get_class_layout("Point");
    check(layout != nullptr, "Point class layout found");
    if (layout) {
        check(layout->fields.size() == 2, "2 fields in layout");
        check(layout->fields[0].name == "x", "first field is x");
        check(layout->fields[1].name == "y", "second field is y");
    }

    // Check function symbols include fields
    auto* symbols = smap.get_function_symbols("Point.new");
    check(symbols != nullptr, "Point.new symbols found");
    if (symbols) {
        check(symbols->fields.size() == 2, "2 fields in symbols");
        check(symbols->arguments.size() == 2, "2 arguments (ax, ay)");
    }
}

static void test_auto_source_map_multi_class() {
    std::cout << "\n--- Auto Source Map: Multiple Classes ---\n";

    auto main_info = parse_jack_class(
        "class Main {\n"
        "  function void main() {\n"
        "    var Point p;\n"
        "    let p = Point.new(3, 4);\n"
        "    return;\n"
        "  }\n"
        "}\n",
        "Main.jack"
    );

    auto point_info = parse_jack_class(
        "class Point {\n"
        "  field int x, y;\n"
        "  constructor Point new(int ax, int ay) {\n"
        "    let x = ax;\n"
        "    let y = ay;\n"
        "    return this;\n"
        "  }\n"
        "}\n",
        "Point.jack"
    );

    std::string vm_source =
        "function Main.main 1\n"
        "push constant 3\n"
        "push constant 4\n"
        "call Point.new 2\n"
        "pop local 0\n"
        "push constant 0\n"
        "return\n"
        "function Point.new 0\n"
        "push constant 2\n"
        "call Memory.alloc 1\n"
        "pop pointer 0\n"
        "push argument 0\n"
        "pop this 0\n"
        "push argument 1\n"
        "pop this 1\n"
        "push pointer 0\n"
        "return\n";

    VMParser parser;
    parser.parse_string(vm_source, "Program.vm");
    auto program = parser.get_program();

    std::vector<JackClassInfo> classes = {main_info, point_info};
    SourceMap smap = generate_source_map(classes, program.commands);

    // Both functions should have symbols
    check(smap.get_function_symbols("Main.main") != nullptr, "Main.main symbols");
    check(smap.get_function_symbols("Point.new") != nullptr, "Point.new symbols");

    // Class layout for Point should exist
    check(smap.get_class_layout("Point") != nullptr, "Point layout exists");

    // Both functions should have line mappings
    auto names = smap.get_function_names();
    check(names.size() >= 2, "at least 2 functions in source map");
}

// ==============================================================================
// Integration: load_jack on JackDebugger
// ==============================================================================

static void test_debugger_load_jack() {
    std::cout << "\n--- Debugger: load_jack Integration ---\n";

    std::string jack_source =
        "class Main {\n"                      // line 1
        "  function void main() {\n"          // line 2
        "    var int x;\n"                    // line 3
        "    let x = 42;\n"                   // line 4
        "    return;\n"                       // line 5
        "  }\n"
        "}\n";

    std::string vm_source =
        "function Main.main 1\n"
        "push constant 42\n"
        "pop local 0\n"
        "push constant 0\n"
        "return\n";

    JackDebugger dbg;
    std::vector<std::pair<std::string, std::string>> sources = {
        {"Main.jack", jack_source}
    };

    try {
        dbg.load_jack(sources, vm_source, "test");
        check(true, "load_jack succeeded");
    } catch (const std::exception& e) {
        std::cout << "  Exception: " << e.what() << "\n";
        check(false, "load_jack should not throw");
    }

    // Should be in READY state
    check(dbg.get_state() == VMState::READY, "state is READY after load");

    // Step and check we can get source info
    dbg.step();
    auto src = dbg.get_current_source();
    check(src.has_value(), "has source after stepping");
    if (src) {
        check(src->function_name == "Main.main", "function is Main.main");
    }

    // Should have variable info
    auto vars = dbg.get_all_variables();
    check(!vars.empty(), "variables available after stepping");

    // Run to completion
    auto state = dbg.run();
    check(state == VMState::HALTED, "program halts normally");
}

// ==============================================================================
// Error handling tests
// ==============================================================================

static void test_tokenizer_errors() {
    std::cout << "\n--- Tokenizer: Error Handling ---\n";

    // Unterminated string
    bool caught = false;
    try {
        JackTokenizer::tokenize("\"unterminated");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "unterminated string throws ParseError");

    // Integer overflow
    caught = false;
    try {
        JackTokenizer::tokenize("99999");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "integer overflow throws ParseError");
}

static void test_parser_errors() {
    std::cout << "\n--- Parser: Error Handling ---\n";

    // Missing class keyword
    bool caught = false;
    try {
        parse_jack_class("Main { }");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "missing 'class' keyword throws ParseError");

    // Missing class name
    caught = false;
    try {
        parse_jack_class("class { }");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "missing class name throws ParseError");
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "Jack Parser Test Suite\n";
    std::cout << "========================================\n";

    // Tokenizer tests
    test_tokenizer_keywords();
    test_tokenizer_all_keywords();
    test_tokenizer_literals();
    test_tokenizer_symbols();
    test_tokenizer_comments();
    test_tokenizer_line_numbers();
    test_tokenizer_errors();

    // Declaration parser tests
    test_parser_simple_class();
    test_parser_fields_and_statics();
    test_parser_parameters();
    test_parser_method_arg_shift();
    test_parser_statement_scanning();
    test_parser_multiple_var_decls();
    test_parser_multiple_subroutines();
    test_parser_errors();

    // Auto source map tests
    test_auto_source_map_basic();
    test_auto_source_map_with_fields();
    test_auto_source_map_multi_class();

    // Integration tests
    test_debugger_load_jack();

    std::cout << "\n========================================\n";
    std::cout << "Results: " << pass_count << "/" << test_count << " passed\n";
    std::cout << "========================================\n";

    return had_failure ? 1 : 0;
}
