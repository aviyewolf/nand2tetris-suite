// =============================================================================
// Embind Bindings — expose all four engines to JavaScript
// =============================================================================
// Each engine gets a namespace-style class name (e.g. HDLEngine, CPUEngine).
// std::optional<T> returns are wrapped in helper functions that return
// emscripten::val (either the value or val::null()).
// =============================================================================

#include <emscripten/bind.h>
#include <emscripten/val.h>

// Engine headers
#include "hdl_engine.hpp"
#include "cpu.hpp"
#include "vm_engine.hpp"
#include "jack_debugger.hpp"

// Supporting headers
#include "types.hpp"
#include "error.hpp"
#include "instruction.hpp"
#include "vm_command.hpp"
#include "source_map.hpp"
#include "object_inspector.hpp"

using namespace emscripten;
using namespace n2t;

// =============================================================================
// Helper: convert std::optional<T> → emscripten::val
// =============================================================================

template <typename T>
static val optional_to_val(const std::optional<T>& opt) {
    if (opt.has_value()) return val(opt.value());
    return val::null();
}

// =============================================================================
// Helper: convert std::vector<T> → JS array via val::array()
// =============================================================================

template <typename T>
static val vec_to_array(const std::vector<T>& v) {
    val arr = val::array();
    for (size_t i = 0; i < v.size(); ++i)
        arr.call<void>("push", v[i]);
    return arr;
}

static val vec_word_to_array(const std::vector<Word>& v) {
    val arr = val::array();
    for (size_t i = 0; i < v.size(); ++i)
        arr.call<void>("push", static_cast<unsigned>(v[i]));
    return arr;
}

static val vec_string_to_array(const std::vector<std::string>& v) {
    val arr = val::array();
    for (size_t i = 0; i < v.size(); ++i)
        arr.call<void>("push", v[i]);
    return arr;
}

// =============================================================================
// VMCommand → string helper (variant can't be bound directly)
// =============================================================================

static std::string vm_command_to_string_at(const VMEngine& eng, size_t idx) {
    const VMCommand* cmd = eng.get_command(idx);
    if (!cmd) return "";
    return command_to_string(*cmd);
}

static std::string vm_current_command_string(const VMEngine& eng) {
    const VMCommand* cmd = eng.get_current_command();
    if (!cmd) return "";
    return command_to_string(*cmd);
}

// =============================================================================
// CallFrame helpers (bound as plain objects)
// =============================================================================

static val call_frame_to_val(const CallFrame& f) {
    val obj = val::object();
    obj.set("return_address", static_cast<unsigned>(f.return_address));
    obj.set("function_name", f.function_name);
    obj.set("num_args", f.num_args);
    obj.set("num_locals", f.num_locals);
    obj.set("caller_file", f.caller_file);
    obj.set("caller_line", static_cast<unsigned>(f.caller_line));
    return obj;
}

static val vm_call_stack(const VMEngine& eng) {
    val arr = val::array();
    for (auto& f : eng.get_call_stack())
        arr.call<void>("push", call_frame_to_val(f));
    return arr;
}

// =============================================================================
// Jack helpers
// =============================================================================

static val jack_call_frame_to_val(const JackCallFrame& f) {
    val obj = val::object();
    obj.set("function_name", f.function_name);
    obj.set("jack_file", f.jack_file);
    obj.set("jack_line", static_cast<unsigned>(f.jack_line));
    obj.set("vm_command_index", static_cast<unsigned>(f.vm_command_index));
    return obj;
}

static val jack_call_stack(const JackDebugger& dbg) {
    val arr = val::array();
    for (auto& f : dbg.get_jack_call_stack())
        arr.call<void>("push", jack_call_frame_to_val(f));
    return arr;
}

static val jack_current_source(const JackDebugger& dbg) {
    auto src = dbg.get_current_source();
    if (!src) return val::null();
    val obj = val::object();
    obj.set("jack_file", src->jack_file);
    obj.set("jack_line", static_cast<unsigned>(src->jack_line));
    obj.set("vm_command_index", static_cast<unsigned>(src->vm_command_index));
    obj.set("function_name", src->function_name);
    return obj;
}

static val jack_variable_to_val(const JackVariableValue& v) {
    val obj = val::object();
    obj.set("name", v.name);
    obj.set("type_name", v.type_name);
    obj.set("kind", static_cast<int>(v.kind));
    obj.set("index", v.index);
    obj.set("raw_value", static_cast<unsigned>(v.raw_value));
    obj.set("signed_value", v.signed_value);
    return obj;
}

static val jack_get_variable(const JackDebugger& dbg, const std::string& name) {
    auto v = dbg.get_variable(name);
    if (!v) return val::null();
    return jack_variable_to_val(*v);
}

static val jack_get_all_variables(const JackDebugger& dbg) {
    val arr = val::array();
    for (auto& v : dbg.get_all_variables())
        arr.call<void>("push", jack_variable_to_val(v));
    return arr;
}

static val jack_evaluate(const JackDebugger& dbg, const std::string& expr) {
    auto r = dbg.evaluate(expr);
    if (!r) return val::null();
    return val(*r);
}

static val jack_breakpoints(const JackDebugger& dbg) {
    val arr = val::array();
    for (auto& [file, line] : dbg.get_breakpoints()) {
        val obj = val::object();
        obj.set("file", file);
        obj.set("line", static_cast<unsigned>(line));
        arr.call<void>("push", obj);
    }
    return arr;
}

static val inspected_object_to_val(const InspectedObject& o) {
    val obj = val::object();
    obj.set("class_name", o.class_name);
    obj.set("heap_address", static_cast<unsigned>(o.heap_address));
    val fields = val::array();
    for (auto& f : o.fields) {
        val fv = val::object();
        fv.set("field_name", f.field_name);
        fv.set("type_name", f.type_name);
        fv.set("raw_value", static_cast<unsigned>(f.raw_value));
        fv.set("signed_value", f.signed_value);
        fv.set("is_reference", f.is_reference);
        fields.call<void>("push", fv);
    }
    obj.set("fields", fields);
    return obj;
}

static val jack_inspect_object(const JackDebugger& dbg, unsigned addr, const std::string& cls) {
    return inspected_object_to_val(dbg.inspect_object(static_cast<Address>(addr), cls));
}

static val jack_inspect_this(const JackDebugger& dbg) {
    return inspected_object_to_val(dbg.inspect_this());
}

static val jack_inspect_array(const JackDebugger& dbg, unsigned addr, unsigned len) {
    auto a = dbg.inspect_array(static_cast<Address>(addr), static_cast<size_t>(len));
    val obj = val::object();
    obj.set("heap_address", static_cast<unsigned>(a.heap_address));
    obj.set("length", static_cast<unsigned>(a.length));
    obj.set("elements", vec_word_to_array(a.elements));
    return obj;
}

// =============================================================================
// Jack error helper
// =============================================================================

static std::string jack_get_error_message(const JackDebugger& dbg) {
    return dbg.engine().get_error_message();
}

// =============================================================================
// CPU helpers
// =============================================================================

static val cpu_stats(const CPUEngine& eng) {
    auto& s = eng.get_stats();
    val obj = val::object();
    obj.set("instructions_executed", static_cast<double>(s.instructions_executed));
    obj.set("a_instruction_count", static_cast<double>(s.a_instruction_count));
    obj.set("c_instruction_count", static_cast<double>(s.c_instruction_count));
    obj.set("jump_count", static_cast<double>(s.jump_count));
    obj.set("memory_reads", static_cast<double>(s.memory_reads));
    obj.set("memory_writes", static_cast<double>(s.memory_writes));
    return obj;
}

static val cpu_breakpoints(const CPUEngine& eng) {
    val arr = val::array();
    for (auto bp : eng.get_breakpoints())
        arr.call<void>("push", static_cast<unsigned>(bp));
    return arr;
}

static val cpu_disassemble_range(const CPUEngine& eng, unsigned start, unsigned end) {
    return vec_string_to_array(eng.disassemble_range(
        static_cast<Address>(start), static_cast<Address>(end)));
}

static val cpu_current_instruction(const CPUEngine& eng) {
    auto instr = eng.get_current_instruction();
    val obj = val::object();
    obj.set("type", static_cast<int>(instr.type));
    obj.set("value", static_cast<unsigned>(instr.value));
    obj.set("reads_memory", instr.reads_memory);
    if (instr.type == InstructionType::C_INSTRUCTION) {
        obj.set("comp", static_cast<int>(instr.comp));
        obj.set("dest_a", instr.dest.a);
        obj.set("dest_d", instr.dest.d);
        obj.set("dest_m", instr.dest.m);
        obj.set("jump", static_cast<int>(instr.jump));
    }
    return obj;
}

// =============================================================================
// HDL helpers
// =============================================================================

static val hdl_stats(const HDLEngine& eng) {
    auto& s = eng.get_stats();
    val obj = val::object();
    obj.set("eval_count", static_cast<double>(s.eval_count));
    obj.set("output_rows", static_cast<double>(s.output_rows));
    return obj;
}

// =============================================================================
// EMBIND DECLARATIONS
// =============================================================================

EMSCRIPTEN_BINDINGS(n2t_enums) {
    // -- Enums ----------------------------------------------------------------
    enum_<HDLState>("HDLState")
        .value("READY",   HDLState::READY)
        .value("RUNNING", HDLState::RUNNING)
        .value("PAUSED",  HDLState::PAUSED)
        .value("HALTED",  HDLState::HALTED)
        .value("ERROR",   HDLState::ERROR);

    enum_<CPUState>("CPUState")
        .value("READY",   CPUState::READY)
        .value("RUNNING", CPUState::RUNNING)
        .value("PAUSED",  CPUState::PAUSED)
        .value("HALTED",  CPUState::HALTED)
        .value("ERROR",   CPUState::ERROR);

    enum_<VMState>("VMState")
        .value("READY",   VMState::READY)
        .value("RUNNING", VMState::RUNNING)
        .value("PAUSED",  VMState::PAUSED)
        .value("HALTED",  VMState::HALTED)
        .value("ERROR",   VMState::ERROR);

    enum_<SegmentType>("SegmentType")
        .value("LOCAL",    SegmentType::LOCAL)
        .value("ARGUMENT", SegmentType::ARGUMENT)
        .value("THIS",     SegmentType::THIS)
        .value("THAT",     SegmentType::THAT)
        .value("CONSTANT", SegmentType::CONSTANT)
        .value("STATIC",   SegmentType::STATIC)
        .value("TEMP",     SegmentType::TEMP)
        .value("POINTER",  SegmentType::POINTER);
}

EMSCRIPTEN_BINDINGS(n2t_hdl) {
    // -- HDL Engine -----------------------------------------------------------
    class_<HDLEngine>("HDLEngine")
        .constructor<>()
        // Loading
        .function("loadString",   &HDLEngine::load_hdl_string)
        .function("reset",        &HDLEngine::reset)
        // Direct manipulation
        .function("setInput",     &HDLEngine::set_input)
        .function("getOutput",    &HDLEngine::get_output)
        .function("eval",         &HDLEngine::eval)
        .function("tick",         &HDLEngine::tick)
        .function("tock",         &HDLEngine::tock)
        // Test execution
        .function("runTestString", &HDLEngine::run_test_string)
        .function("prepareTest",  &HDLEngine::prepare_test)
        .function("stepTest",     &HDLEngine::step_test)
        // Output
        .function("getOutputTable",      &HDLEngine::get_output_table)
        .function("hasComparisonError",  &HDLEngine::has_comparison_error)
        // State
        .function("getState",        &HDLEngine::get_state)
        .function("getStats",        &hdl_stats)
        .function("getErrorMessage", &HDLEngine::get_error_message);
}

EMSCRIPTEN_BINDINGS(n2t_cpu) {
    // -- CPU Engine -----------------------------------------------------------
    class_<CPUEngine>("CPUEngine")
        .constructor<>()
        // Loading
        .function("loadString", &CPUEngine::load_string)
        .function("reset",      &CPUEngine::reset)
        // Execution
        .function("run",        &CPUEngine::run)
        .function("runFor",     &CPUEngine::run_for)
        .function("step",       &CPUEngine::step)
        .function("pause",      &CPUEngine::pause)
        .function("getState",   &CPUEngine::get_state)
        // Registers
        .function("getA",       &CPUEngine::get_a)
        .function("getD",       &CPUEngine::get_d)
        .function("getPC",      &CPUEngine::get_pc)
        // Memory
        .function("readRam",    &CPUEngine::read_ram)
        .function("writeRam",   &CPUEngine::write_ram)
        .function("readRom",    &CPUEngine::read_rom)
        .function("romSize",    &CPUEngine::rom_size)
        // I/O
        .function("getKeyboard",  &CPUEngine::get_keyboard)
        .function("setKeyboard",  &CPUEngine::set_keyboard)
        // Breakpoints
        .function("addBreakpoint",    &CPUEngine::add_breakpoint)
        .function("removeBreakpoint", &CPUEngine::remove_breakpoint)
        .function("clearBreakpoints", &CPUEngine::clear_breakpoints)
        .function("hasBreakpoint",    &CPUEngine::has_breakpoint)
        .function("getBreakpoints",   &cpu_breakpoints)
        // Disassembly
        .function("disassemble",         &CPUEngine::disassemble)
        .function("disassembleRange",    &cpu_disassemble_range)
        .function("getCurrentInstruction", &cpu_current_instruction)
        // Stats & errors
        .function("getStats",         &cpu_stats)
        .function("getErrorMessage",  &CPUEngine::get_error_message)
        .function("getErrorLocation", &CPUEngine::get_error_location);

    // CPU Stats (read via plain object)
    class_<CPUStats>("CPUStats")
        .property("instructions_executed", &CPUStats::instructions_executed)
        .property("a_instruction_count",   &CPUStats::a_instruction_count)
        .property("c_instruction_count",   &CPUStats::c_instruction_count)
        .property("jump_count",            &CPUStats::jump_count)
        .property("memory_reads",          &CPUStats::memory_reads)
        .property("memory_writes",         &CPUStats::memory_writes);
}

EMSCRIPTEN_BINDINGS(n2t_vm) {
    // -- VM Engine ------------------------------------------------------------
    class_<VMEngine>("VMEngine")
        .constructor<>()
        // Loading
        .function("loadString",    &VMEngine::load_string)
        .function("setEntryPoint", &VMEngine::set_entry_point)
        .function("reset",         &VMEngine::reset)
        // Execution
        .function("run",       &VMEngine::run)
        .function("runFor",    &VMEngine::run_for)
        .function("step",      &VMEngine::step)
        .function("stepOver",  &VMEngine::step_over)
        .function("stepOut",   &VMEngine::step_out)
        .function("pause",     &VMEngine::pause)
        .function("getState",  &VMEngine::get_state)
        // State
        .function("getPC",              &VMEngine::get_pc)
        .function("getCommandCount",    &VMEngine::get_command_count)
        .function("getCurrentFunction", &VMEngine::get_current_function)
        .function("getCallStack",       &vm_call_stack)
        .function("getStack",           select_overload<std::vector<Word>() const>(&VMEngine::get_stack),
                  allow_raw_pointers())
        .function("getSP",              &VMEngine::get_sp)
        .function("getSegment",         &VMEngine::get_segment)
        // Commands
        .function("commandToString",       &vm_command_to_string_at)
        .function("currentCommandString",  &vm_current_command_string)
        // Memory
        .function("readRam",   &VMEngine::read_ram)
        .function("writeRam",  &VMEngine::write_ram)
        // Breakpoints
        .function("addBreakpoint",         &VMEngine::add_breakpoint)
        .function("addFunctionBreakpoint", &VMEngine::add_function_breakpoint)
        .function("removeBreakpoint",      &VMEngine::remove_breakpoint)
        .function("clearBreakpoints",      &VMEngine::clear_breakpoints)
        // I/O
        .function("getKeyboard",  &VMEngine::get_keyboard)
        .function("setKeyboard",  &VMEngine::set_keyboard)
        // Error
        .function("getErrorMessage",  &VMEngine::get_error_message)
        .function("getErrorLocation", &VMEngine::get_error_location);
}

// Helper: load Jack sources from JS array of [filename, source] pairs
static void jack_load_with_sources(JackDebugger& dbg, val jack_sources_val,
                                   const std::string& vm_source,
                                   const std::string& name) {
    std::vector<std::pair<std::string, std::string>> jack_sources;
    unsigned len = jack_sources_val["length"].as<unsigned>();
    for (unsigned i = 0; i < len; ++i) {
        val pair = jack_sources_val[i];
        std::string filename = pair[0].as<std::string>();
        std::string source = pair[1].as<std::string>();
        jack_sources.push_back({filename, source});
    }
    dbg.load_jack(jack_sources, vm_source, name);
}

EMSCRIPTEN_BINDINGS(n2t_jack) {
    // -- Jack Debugger --------------------------------------------------------
    class_<JackDebugger>("JackDebugger")
        .constructor<>()
        // Loading
        .function("load",           &JackDebugger::load)
        .function("loadVM",         &JackDebugger::load_vm)
        .function("loadSourceMap",  &JackDebugger::load_source_map)
        .function("loadWithSources", &jack_load_with_sources)
        .function("setEntryPoint",  &JackDebugger::set_entry_point)
        .function("reset",          &JackDebugger::reset)
        // Execution
        .function("step",     &JackDebugger::step)
        .function("stepOver", &JackDebugger::step_over)
        .function("stepOut",  &JackDebugger::step_out)
        .function("run",      &JackDebugger::run)
        .function("runFor",   &JackDebugger::run_for)
        .function("pause",    &JackDebugger::pause)
        // State
        .function("getState",           &JackDebugger::get_state)
        .function("isRunning",          &JackDebugger::is_running)
        .function("getCurrentSource",   &jack_current_source)
        .function("getCurrentFunction", &JackDebugger::get_current_function)
        .function("getCallStack",       &jack_call_stack)
        // Breakpoints
        .function("addBreakpoint",    &JackDebugger::add_breakpoint)
        .function("removeBreakpoint", &JackDebugger::remove_breakpoint)
        .function("clearBreakpoints", &JackDebugger::clear_breakpoints)
        .function("getBreakpoints",   &jack_breakpoints)
        // Variables
        .function("getVariable",     &jack_get_variable)
        .function("getAllVariables",  &jack_get_all_variables)
        .function("evaluate",        &jack_evaluate)
        // Object inspection
        .function("inspectObject", &jack_inspect_object)
        .function("inspectThis",   &jack_inspect_this)
        .function("inspectArray",  &jack_inspect_array)
        // Error
        .function("getErrorMessage", &jack_get_error_message);
}
