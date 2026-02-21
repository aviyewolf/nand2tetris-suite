// ==============================================================================
// Jack Debugger Tests
// ==============================================================================
// Tests for source map, object inspector, and Jack debugger.
// Uses assertions rather than a test framework for consistency.
// ==============================================================================

#include "jack_debugger.hpp"
#include "source_map.hpp"
#include "object_inspector.hpp"
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
// Source Map Tests
// ==============================================================================

static void test_source_map_parsing() {
    std::cout << "\n--- Source Map Parsing ---\n";

    SourceMap smap;
    smap.load_string(
        "MAP Main:10 -> 0 [Main.main]\n"
        "MAP Main:11 -> 3 [Main.main]\n"
        "MAP Main:12 -> 7 [Main.main]\n"
        "FUNC Main.main\n"
        "VAR local int sum 0\n"
        "VAR local int i 1\n"
        "VAR argument int n 0\n"
        "CLASS Point\n"
        "FIELD int x\n"
        "FIELD int y\n"
    );

    check(!smap.empty(), "source map loaded");
    check(smap.entries().size() == 3, "3 map entries parsed");

    // Check MAP entries
    auto entry = smap.get_entry_for_vm(0);
    check(entry.has_value(), "vm index 0 found");
    check(entry->jack_file == "Main", "entry file is Main");
    check(entry->jack_line == 10, "entry line is 10");
    check(entry->function_name == "Main.main", "entry function is Main.main");

    auto entry2 = smap.get_entry_for_vm(3);
    check(entry2.has_value(), "vm index 3 found");
    check(entry2->jack_line == 11, "entry2 line is 11");

    // Check function symbols
    auto* symbols = smap.get_function_symbols("Main.main");
    check(symbols != nullptr, "Main.main symbols found");
    check(symbols->class_name == "Main", "class name is Main");
    check(symbols->locals.size() == 2, "2 local variables");
    check(symbols->locals[0].name == "sum", "first local is sum");
    check(symbols->locals[0].type_name == "int", "sum type is int");
    check(symbols->locals[0].index == 0, "sum index is 0");
    check(symbols->locals[1].name == "i", "second local is i");
    check(symbols->locals[1].index == 1, "i index is 1");
    check(symbols->arguments.size() == 1, "1 argument");
    check(symbols->arguments[0].name == "n", "argument is n");

    // Check class layout
    auto* layout = smap.get_class_layout("Point");
    check(layout != nullptr, "Point layout found");
    check(layout->fields.size() == 2, "Point has 2 fields");
    check(layout->fields[0].name == "x", "first field is x");
    check(layout->fields[0].type_name == "int", "x type is int");
    check(layout->fields[1].name == "y", "second field is y");
}

static void test_source_map_queries() {
    std::cout << "\n--- Source Map Queries ---\n";

    SourceMap smap;
    smap.load_string(
        "MAP Main:10 -> 0 [Main.main]\n"
        "MAP Main:10 -> 1 [Main.main]\n"
        "MAP Main:10 -> 2 [Main.main]\n"
        "MAP Main:11 -> 3 [Main.main]\n"
        "MAP Other:5 -> 10 [Other.foo]\n"
    );

    // Forward lookup
    auto entry = smap.get_entry_for_vm(0);
    check(entry.has_value(), "forward lookup hit");
    check(entry->jack_file == "Main", "forward lookup file");
    check(entry->jack_line == 10, "forward lookup line");

    // Forward lookup miss
    auto miss = smap.get_entry_for_vm(99);
    check(!miss.has_value(), "forward lookup miss");

    // Reverse lookup
    auto vm_idx = smap.get_vm_index_for_line("Main", 10);
    check(vm_idx.has_value(), "reverse lookup hit");
    check(*vm_idx == 0, "reverse lookup returns first vm index");

    // Reverse lookup miss
    auto vm_miss = smap.get_vm_index_for_line("Main", 99);
    check(!vm_miss.has_value(), "reverse lookup miss");

    // Get all VM indices for a line
    auto all_indices = smap.get_all_vm_indices_for_line("Main", 10);
    check(all_indices.size() == 3, "3 vm indices for Main:10");
    check(all_indices[0] == 0, "first index is 0");
    check(all_indices[1] == 1, "second index is 1");
    check(all_indices[2] == 2, "third index is 2");

    // Cross-file lookup
    auto other_entry = smap.get_entry_for_vm(10);
    check(other_entry.has_value(), "cross-file lookup hit");
    check(other_entry->jack_file == "Other", "cross-file is Other");
}

static void test_source_map_errors() {
    std::cout << "\n--- Source Map Error Handling ---\n";

    SourceMap smap;

    // Empty source map
    smap.load_string("");
    check(smap.empty(), "empty source map");

    // Comments and blank lines
    smap.load_string(
        "# This is a comment\n"
        "\n"
        "   \n"
        "MAP Main:1 -> 0 [Main.main]\n"
    );
    check(smap.entries().size() == 1, "comments and blanks skipped");

    // Unknown function
    check(smap.get_function_symbols("Nonexistent") == nullptr,
          "unknown function returns null");

    // Unknown class
    check(smap.get_class_layout("Nonexistent") == nullptr,
          "unknown class returns null");

    // Invalid directive
    bool caught = false;
    try {
        SourceMap bad;
        bad.load_string("INVALID line\n");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "invalid directive throws ParseError");

    // VAR without FUNC
    caught = false;
    try {
        SourceMap bad;
        bad.load_string("VAR local int x 0\n");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "VAR without FUNC throws ParseError");

    // FIELD without CLASS
    caught = false;
    try {
        SourceMap bad;
        bad.load_string("FIELD int x\n");
    } catch (const ParseError&) {
        caught = true;
    }
    check(caught, "FIELD without CLASS throws ParseError");
}

static void test_source_map_metadata() {
    std::cout << "\n--- Source Map Metadata ---\n";

    SourceMap smap;
    smap.load_string(
        "FUNC Main.main\n"
        "VAR local int x 0\n"
        "FUNC Math.add\n"
        "VAR argument int a 0\n"
        "CLASS Point\n"
        "FIELD int x\n"
        "CLASS Circle\n"
        "FIELD int radius\n"
    );

    auto func_names = smap.get_function_names();
    check(func_names.size() == 2, "2 function names");

    auto class_names = smap.get_class_names();
    check(class_names.size() == 2, "2 class names");
}

// ==============================================================================
// Jack Stepping Tests
// ==============================================================================

// A VM program where Jack line 10 maps to VM commands 0-2,
// Jack line 11 maps to VM commands 3-4, etc.
static const char* STEPPING_SMAP =
    "MAP Main:10 -> 1 [Main.main]\n"
    "MAP Main:10 -> 2 [Main.main]\n"
    "MAP Main:11 -> 3 [Main.main]\n"
    "MAP Main:12 -> 4 [Main.main]\n"
    "MAP Main:12 -> 5 [Main.main]\n"
    "FUNC Main.main\n"
    "VAR local int x 0\n"
    "VAR local int y 1\n";

static const char* STEPPING_VM =
    "function Main.main 2\n"   // cmd 0 - not mapped
    "push constant 10\n"       // cmd 1 - Main:10
    "pop local 0\n"            // cmd 2 - Main:10
    "push constant 20\n"       // cmd 3 - Main:11
    "push local 0\n"           // cmd 4 - Main:12
    "pop local 1\n"            // cmd 5 - Main:12
    "push local 1\n"           // cmd 6 - not mapped, will halt
    "return\n";                // cmd 7

static void test_jack_step() {
    std::cout << "\n--- Jack Step ---\n";

    JackDebugger dbg;
    dbg.load(STEPPING_VM, STEPPING_SMAP, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Step should advance to first Jack line (Main:10, cmd 1)
    // Starting at cmd 0 (function Main.main), no source mapping
    VMState state = dbg.step();
    auto src = dbg.get_current_source();
    // After stepping, we should be at a different line than where we started
    // Since cmd 0 has no mapping, one step should execute cmd 0 (unmapped)
    // and advance. The step logic: when no current source and no new source, do one VM step.
    // So first step from cmd 0 (unmapped) -> cmd 1 (Main:10)
    check(state != VMState::ERROR, "step didn't error");

    // Now step from Main:10 (cmds 1-2) to Main:11 (cmd 3)
    state = dbg.step();
    src = dbg.get_current_source();
    check(src.has_value(), "source after step to line 11");
    if (src) {
        check(src->jack_line == 11, "stepped to line 11");
    }

    // Step from Main:11 (cmd 3) to Main:12 (cmds 4-5)
    state = dbg.step();
    src = dbg.get_current_source();
    check(src.has_value(), "source after step to line 12");
    if (src) {
        check(src->jack_line == 12, "stepped to line 12");
    }
}

static void test_jack_step_over() {
    std::cout << "\n--- Jack Step Over ---\n";

    // Program with a function call
    const char* vm =
        "function Sys.init 0\n"     // cmd 0
        "push constant 5\n"         // cmd 1 - Init:10
        "call Math.double 1\n"      // cmd 2 - Init:10
        "pop temp 0\n"              // cmd 3 - Init:11
        "push temp 0\n"             // cmd 4 - Init:12
        "return\n"                  // cmd 5
        "function Math.double 0\n"  // cmd 6
        "push argument 0\n"         // cmd 7 - Math:5
        "push argument 0\n"         // cmd 8 - Math:5
        "add\n"                     // cmd 9 - Math:6
        "return\n";                 // cmd 10 - Math:7

    const char* smap =
        "MAP Init:10 -> 1 [Sys.init]\n"
        "MAP Init:10 -> 2 [Sys.init]\n"
        "MAP Init:11 -> 3 [Sys.init]\n"
        "MAP Init:12 -> 4 [Sys.init]\n"
        "MAP Math:5 -> 7 [Math.double]\n"
        "MAP Math:5 -> 8 [Math.double]\n"
        "MAP Math:6 -> 9 [Math.double]\n"
        "MAP Math:7 -> 10 [Math.double]\n"
        "FUNC Sys.init\n"
        "FUNC Math.double\n"
        "VAR argument int n 0\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.reset();

    // Step to cmd 1 (first mapped line in Sys.init)
    dbg.step();  // execute cmd 0 (function Sys.init, unmapped) -> now at cmd 1

    // Step over should skip the Math.double call and land on Init:11
    dbg.step_over();
    auto src = dbg.get_current_source();
    check(src.has_value(), "source after step_over");
    if (src) {
        check(src->jack_line == 11, "step_over skipped call, landed on Init:11");
        check(src->jack_file == "Init", "step_over stayed in Init file");
    }
}

static void test_jack_step_out() {
    std::cout << "\n--- Jack Step Out ---\n";

    const char* vm =
        "function Sys.init 0\n"     // cmd 0
        "push constant 5\n"         // cmd 1 - Init:10
        "call Math.double 1\n"      // cmd 2 - Init:10
        "pop temp 0\n"              // cmd 3 - Init:11
        "push temp 0\n"             // cmd 4 - Init:12
        "return\n"                  // cmd 5
        "function Math.double 0\n"  // cmd 6
        "push argument 0\n"         // cmd 7 - Math:5
        "push argument 0\n"         // cmd 8 - Math:5
        "add\n"                     // cmd 9 - Math:6
        "return\n";                 // cmd 10 - Math:7

    const char* smap =
        "MAP Init:10 -> 1 [Sys.init]\n"
        "MAP Init:10 -> 2 [Sys.init]\n"
        "MAP Init:11 -> 3 [Sys.init]\n"
        "MAP Init:12 -> 4 [Sys.init]\n"
        "MAP Math:5 -> 7 [Math.double]\n"
        "MAP Math:5 -> 8 [Math.double]\n"
        "MAP Math:6 -> 9 [Math.double]\n"
        "MAP Math:7 -> 10 [Math.double]\n"
        "FUNC Sys.init\n"
        "FUNC Math.double\n"
        "VAR argument int n 0\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.reset();

    // Run until we're inside Math.double
    // Use run_for to advance past the call
    dbg.engine().step();  // cmd 0: function Sys.init
    dbg.engine().step();  // cmd 1: push constant 5
    dbg.engine().step();  // cmd 2: call Math.double -> enters Math.double
    dbg.engine().step();  // cmd 6: function Math.double
    dbg.engine().step();  // cmd 7: push argument 0

    // Now we're inside Math.double at cmd 8
    // step_out should return to Sys.init
    dbg.step_out();
    auto src = dbg.get_current_source();
    check(src.has_value(), "source after step_out");
    if (src) {
        check(src->jack_file == "Init", "step_out returned to Init");
    }
}

// ==============================================================================
// Breakpoint Tests
// ==============================================================================

static void test_jack_breakpoints() {
    std::cout << "\n--- Jack Breakpoints ---\n";

    JackDebugger dbg;
    dbg.load(STEPPING_VM, STEPPING_SMAP, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Add breakpoint at Main:11
    bool added = dbg.add_breakpoint("Main", 11);
    check(added, "breakpoint added at Main:11");

    // Check breakpoint exists
    check(dbg.has_breakpoint("Main", 11), "has breakpoint at Main:11");
    check(!dbg.has_breakpoint("Main", 10), "no breakpoint at Main:10");

    // Get breakpoints
    auto bps = dbg.get_breakpoints();
    check(bps.size() == 1, "1 breakpoint total");
    check(bps[0].first == "Main" && bps[0].second == 11,
          "breakpoint is Main:11");

    // Run should stop at breakpoint
    VMState state = dbg.run();
    check(state == VMState::PAUSED, "run paused at breakpoint");
    check(dbg.get_pause_reason() == JackPauseReason::BREAKPOINT,
          "paused for breakpoint");

    auto src = dbg.get_current_source();
    check(src.has_value(), "source at breakpoint");
    if (src) {
        check(src->jack_line == 11, "stopped at line 11");
    }

    // Remove breakpoint and continue
    bool removed = dbg.remove_breakpoint("Main", 11);
    check(removed, "breakpoint removed");
    check(!dbg.has_breakpoint("Main", 11), "breakpoint gone");

    state = dbg.run();
    check(state == VMState::HALTED, "run completed after removing breakpoint");

    // Try adding breakpoint at unmapped line
    dbg.reset();
    bool fail = dbg.add_breakpoint("Main", 999);
    check(!fail, "breakpoint at unmapped line fails");
}

static void test_jack_clear_breakpoints() {
    std::cout << "\n--- Clear Breakpoints ---\n";

    SourceMap smap;
    smap.load_string(STEPPING_SMAP);

    JackDebugger dbg;
    dbg.load(STEPPING_VM, STEPPING_SMAP, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    dbg.add_breakpoint("Main", 10);
    dbg.add_breakpoint("Main", 11);
    dbg.add_breakpoint("Main", 12);

    check(dbg.get_breakpoints().size() == 3, "3 breakpoints added");

    dbg.clear_breakpoints();
    check(dbg.get_breakpoints().empty(), "all breakpoints cleared");
}

// ==============================================================================
// Variable Inspection Tests
// ==============================================================================

static void test_variable_inspection() {
    std::cout << "\n--- Variable Inspection ---\n";

    const char* vm =
        "function Main.main 2\n"   // cmd 0
        "push constant 42\n"       // cmd 1 - Main:10
        "pop local 0\n"            // cmd 2 - Main:10
        "push constant 99\n"       // cmd 3 - Main:11
        "pop local 1\n"            // cmd 4 - Main:11
        "push local 0\n"           // cmd 5 - Main:12
        "return\n";                // cmd 6

    const char* smap =
        "MAP Main:10 -> 1 [Main.main]\n"
        "MAP Main:10 -> 2 [Main.main]\n"
        "MAP Main:11 -> 3 [Main.main]\n"
        "MAP Main:11 -> 4 [Main.main]\n"
        "MAP Main:12 -> 5 [Main.main]\n"
        "FUNC Main.main\n"
        "VAR local int x 0\n"
        "VAR local int y 1\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Execute up to cmd 5 (after both locals are set)
    for (int i = 0; i < 5; i++) {
        dbg.engine().step();
    }

    // Now we should be at cmd 5, locals set
    auto x = dbg.get_variable("x");
    check(x.has_value(), "variable x found");
    if (x) {
        check(x->signed_value == 42, "x value is 42");
        check(x->kind == JackVarKind::LOCAL, "x is local");
        check(x->type_name == "int", "x type is int");
    }

    auto y = dbg.get_variable("y");
    check(y.has_value(), "variable y found");
    if (y) {
        check(y->signed_value == 99, "y value is 99");
    }

    // Unknown variable
    auto z = dbg.get_variable("z");
    check(!z.has_value(), "unknown variable returns nullopt");

    // Get all variables
    auto all = dbg.get_all_variables();
    check(all.size() == 2, "2 variables in scope");
}

static void test_argument_inspection() {
    std::cout << "\n--- Argument Inspection ---\n";

    const char* vm =
        "function Sys.init 0\n"     // cmd 0
        "push constant 7\n"         // cmd 1
        "push constant 3\n"         // cmd 2
        "call Math.add 2\n"         // cmd 3
        "return\n"                  // cmd 4
        "function Math.add 1\n"     // cmd 5
        "push argument 0\n"         // cmd 6 - Math:10
        "push argument 1\n"         // cmd 7 - Math:10
        "add\n"                     // cmd 8 - Math:11
        "pop local 0\n"             // cmd 9 - Math:11
        "push local 0\n"            // cmd 10 - Math:12
        "return\n";                 // cmd 11

    const char* smap =
        "MAP Math:10 -> 6 [Math.add]\n"
        "MAP Math:10 -> 7 [Math.add]\n"
        "MAP Math:11 -> 8 [Math.add]\n"
        "MAP Math:11 -> 9 [Math.add]\n"
        "MAP Math:12 -> 10 [Math.add]\n"
        "FUNC Math.add\n"
        "VAR argument int a 0\n"
        "VAR argument int b 1\n"
        "VAR local int result 0\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.reset();

    // Run to inside Math.add (after function setup)
    // cmd 0: function Sys.init
    // cmd 1: push 7
    // cmd 2: push 3
    // cmd 3: call Math.add -> enters
    // cmd 5: function Math.add (sets up locals)
    // cmd 6: push argument 0
    for (int i = 0; i < 6; i++) {
        dbg.engine().step();
    }

    // Now at cmd 6, inside Math.add
    auto a = dbg.get_variable("a");
    check(a.has_value(), "argument a found");
    if (a) {
        check(a->signed_value == 7, "argument a is 7");
        check(a->kind == JackVarKind::ARGUMENT, "a is argument");
    }

    auto b = dbg.get_variable("b");
    check(b.has_value(), "argument b found");
    if (b) {
        check(b->signed_value == 3, "argument b is 3");
    }
}

// ==============================================================================
// Evaluate Tests
// ==============================================================================

static void test_evaluate() {
    std::cout << "\n--- Evaluate ---\n";

    const char* vm =
        "function Main.main 1\n"   // cmd 0
        "push constant 42\n"       // cmd 1
        "pop local 0\n"            // cmd 2
        "push local 0\n"           // cmd 3
        "return\n";                // cmd 4

    const char* smap =
        "MAP Main:10 -> 1 [Main.main]\n"
        "MAP Main:10 -> 2 [Main.main]\n"
        "MAP Main:11 -> 3 [Main.main]\n"
        "FUNC Main.main\n"
        "VAR local int x 0\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Execute to cmd 3
    for (int i = 0; i < 3; i++) {
        dbg.engine().step();
    }

    // Integer literal
    auto val = dbg.evaluate("123");
    check(val.has_value() && *val == 123, "evaluate integer literal");

    // Negative integer
    val = dbg.evaluate("-5");
    check(val.has_value() && *val == -5, "evaluate negative integer");

    // Variable name
    val = dbg.evaluate("x");
    check(val.has_value() && *val == 42, "evaluate variable name");

    // Unknown
    val = dbg.evaluate("unknown_var");
    check(!val.has_value(), "evaluate unknown returns nullopt");
}

// ==============================================================================
// Object Inspection Tests
// ==============================================================================

static void test_object_inspection() {
    std::cout << "\n--- Object Inspection ---\n";

    const char* vm =
        "function Main.main 0\n"   // cmd 0
        "push constant 100\n"      // cmd 1
        "return\n";                // cmd 2

    const char* smap =
        "CLASS Point\n"
        "FIELD int x\n"
        "FIELD int y\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Manually place an object on the heap
    // Point at address 2048 with x=10, y=20
    dbg.engine().write_ram(2048, 10);  // x
    dbg.engine().write_ram(2049, 20);  // y

    auto obj = dbg.inspect_object(2048, "Point");
    check(obj.class_name == "Point", "inspected class is Point");
    check(obj.heap_address == 2048, "heap address is 2048");
    check(obj.fields.size() == 2, "2 fields inspected");
    check(obj.fields[0].field_name == "x", "first field is x");
    check(obj.fields[0].signed_value == 10, "x value is 10");
    check(!obj.fields[0].is_reference, "x is not reference");
    check(obj.fields[1].field_name == "y", "second field is y");
    check(obj.fields[1].signed_value == 20, "y value is 20");

    // Format
    std::string formatted = ObjectInspector::format_object(obj);
    check(!formatted.empty(), "format_object produces output");
    check(formatted.find("Point") != std::string::npos,
          "formatted contains class name");
}

static void test_object_reference_fields() {
    std::cout << "\n--- Object Reference Fields ---\n";

    const char* smap =
        "CLASS Line\n"
        "FIELD Point start\n"
        "FIELD Point end\n";

    SourceMap source_map;
    source_map.load_string(smap);

    VMEngine engine;
    engine.load_string("function Main.main 0\nreturn\n", "test");
    engine.set_entry_point("Main.main");
    engine.reset();

    // Line at heap 2048 with start=3000, end=3100
    engine.write_ram(2048, 3000);
    engine.write_ram(2049, 3100);

    ObjectInspector inspector(engine.memory(), source_map);
    auto obj = inspector.inspect_object(2048, "Line");
    check(obj.fields[0].is_reference, "start is reference");
    check(obj.fields[0].raw_value == 3000, "start points to 3000");
    check(obj.fields[1].is_reference, "end is reference");
}

static void test_array_inspection() {
    std::cout << "\n--- Array Inspection ---\n";

    JackDebugger dbg;
    dbg.load("function Main.main 0\nreturn\n", "", "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Place array at heap address 2048
    dbg.engine().write_ram(2048, 100);
    dbg.engine().write_ram(2049, 200);
    dbg.engine().write_ram(2050, 300);

    auto arr = dbg.inspect_array(2048, 3);
    check(arr.heap_address == 2048, "array address is 2048");
    check(arr.length == 3, "array length is 3");
    check(arr.elements.size() == 3, "3 elements");
    check(arr.elements[0] == 100, "element 0 is 100");
    check(arr.elements[1] == 200, "element 1 is 200");
    check(arr.elements[2] == 300, "element 2 is 300");

    // Format
    std::string formatted = ObjectInspector::format_array(arr);
    check(!formatted.empty(), "format_array produces output");
    check(formatted.find("Array") != std::string::npos,
          "formatted contains Array");
}

// ==============================================================================
// Call Stack Tests
// ==============================================================================

static void test_call_stack() {
    std::cout << "\n--- Call Stack ---\n";

    const char* vm =
        "function Sys.init 0\n"     // cmd 0
        "push constant 5\n"         // cmd 1
        "call Math.double 1\n"      // cmd 2
        "return\n"                  // cmd 3
        "function Math.double 0\n"  // cmd 4
        "push argument 0\n"         // cmd 5
        "push argument 0\n"         // cmd 6
        "add\n"                     // cmd 7
        "return\n";                 // cmd 8

    const char* smap =
        "MAP Init:10 -> 1 [Sys.init]\n"
        "MAP Init:10 -> 2 [Sys.init]\n"
        "MAP Init:11 -> 3 [Sys.init]\n"
        "MAP Math:5 -> 5 [Math.double]\n"
        "MAP Math:5 -> 6 [Math.double]\n"
        "MAP Math:6 -> 7 [Math.double]\n"
        "FUNC Sys.init\n"
        "FUNC Math.double\n"
        "VAR argument int n 0\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.reset();

    // Run into Math.double
    dbg.engine().step();  // cmd 0: function Sys.init
    dbg.engine().step();  // cmd 1: push 5
    dbg.engine().step();  // cmd 2: call Math.double
    dbg.engine().step();  // cmd 4: function Math.double
    dbg.engine().step();  // cmd 5: push argument 0

    auto stack = dbg.get_jack_call_stack();
    // Should have at least 2 frames: bootstrap/Sys.init and Math.double
    check(stack.size() >= 2, "at least 2 call frames");

    // Find Math.double in the stack
    bool found_double = false;
    for (const auto& frame : stack) {
        if (frame.function_name == "Math.double") {
            found_double = true;
        }
    }
    check(found_double, "Math.double in call stack");
}

// ==============================================================================
// Statistics Tests
// ==============================================================================

static void test_statistics() {
    std::cout << "\n--- Statistics ---\n";

    JackDebugger dbg;
    dbg.load(STEPPING_VM, STEPPING_SMAP, "test");
    dbg.set_entry_point("Main.main");
    dbg.reset();

    // Run to completion
    dbg.run();

    const auto& stats = dbg.get_stats();
    check(stats.total_vm_instructions > 0, "total instructions > 0");

    // Reset stats
    dbg.reset_stats();
    check(dbg.get_stats().total_vm_instructions == 0,
          "stats reset to 0");
}

// ==============================================================================
// Edge Cases
// ==============================================================================

static void test_no_source_map() {
    std::cout << "\n--- No Source Map ---\n";

    JackDebugger dbg;
    dbg.load_vm(
        "function Sys.init 0\n"
        "push constant 42\n"
        "return\n",
        "test"
    );
    dbg.reset();

    // Stepping without source map should still work (VM-level stepping)
    VMState state = dbg.step();
    check(state != VMState::ERROR, "step without source map doesn't error");

    // get_current_source returns nullopt
    auto src = dbg.get_current_source();
    check(!src.has_value(), "no source without source map");

    // Variables return empty
    auto vars = dbg.get_all_variables();
    check(vars.empty(), "no variables without source map");

    // Run to completion
    state = dbg.run();
    check(state == VMState::HALTED, "program completes without source map");
}

static void test_inspect_this() {
    std::cout << "\n--- Inspect This ---\n";

    const char* vm =
        "function Sys.init 0\n"    // cmd 0
        "push constant 2048\n"     // cmd 1
        "pop pointer 0\n"          // cmd 2 - sets THIS to 2048
        "push constant 5\n"        // cmd 3
        "call Point.getX 1\n"      // cmd 4
        "return\n"                 // cmd 5
        "function Point.getX 0\n"  // cmd 6
        "push this 0\n"            // cmd 7
        "return\n";                // cmd 8

    const char* smap =
        "MAP Main:10 -> 1 [Sys.init]\n"
        "MAP Main:10 -> 2 [Sys.init]\n"
        "MAP Point:20 -> 7 [Point.getX]\n"
        "FUNC Sys.init\n"
        "FUNC Point.getX\n"
        "CLASS Point\n"
        "FIELD int x\n"
        "FIELD int y\n";

    JackDebugger dbg;
    dbg.load(vm, smap, "test");
    dbg.reset();

    // Run the first step first so bootstrap frame is set up,
    // THEN write to heap (after init won't clear it)
    dbg.engine().step();  // function Sys.init

    // Set up heap: Point at 2048 with x=10, y=20
    dbg.engine().write_ram(2048, 10);
    dbg.engine().write_ram(2049, 20);

    // Continue execution
    dbg.engine().step();  // push constant 2048
    dbg.engine().step();  // pop pointer 0 (THIS = 2048)
    dbg.engine().step();  // push constant 5
    dbg.engine().step();  // call Point.getX
    dbg.engine().step();  // function Point.getX

    // Now inside Point.getX, THIS should be 2048
    auto obj = dbg.inspect_this();
    check(obj.class_name == "Point", "inspect_this class is Point");
    check(obj.fields.size() == 2, "inspect_this has 2 fields");
    check(obj.fields[0].signed_value == 10, "this.x is 10");
    check(obj.fields[1].signed_value == 20, "this.y is 20");
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "=== Jack Debugger Tests ===\n";

    // Source map tests
    test_source_map_parsing();
    test_source_map_queries();
    test_source_map_errors();
    test_source_map_metadata();

    // Stepping tests
    test_jack_step();
    test_jack_step_over();
    test_jack_step_out();

    // Breakpoint tests
    test_jack_breakpoints();
    test_jack_clear_breakpoints();

    // Variable inspection tests
    test_variable_inspection();
    test_argument_inspection();
    test_evaluate();

    // Object inspection tests
    test_object_inspection();
    test_object_reference_fields();
    test_array_inspection();

    // Call stack tests
    test_call_stack();

    // Statistics tests
    test_statistics();

    // Edge cases
    test_no_source_map();
    test_inspect_this();

    std::cout << "\n=== " << pass_count << "/" << test_count
              << " tests passed! ===\n";
    if (had_failure) {
        std::cout << "SOME TESTS FAILED\n";
        return 1;
    }
    return 0;
}
