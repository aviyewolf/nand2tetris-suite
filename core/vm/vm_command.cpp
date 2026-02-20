// ==============================================================================
// VM Command Implementation
// ==============================================================================

#include "vm_command.hpp"

namespace n2t {

LineNumber get_source_line(const VMCommand& cmd) {
    return std::visit([](const auto& c) -> LineNumber {
        return c.source_line;
    }, cmd);
}

std::string command_to_string(const VMCommand& cmd) {
    return std::visit([](const auto& c) -> std::string {
        using T = std::decay_t<decltype(c)>;

        if constexpr (std::is_same_v<T, ArithmeticCommand>) {
            return arithmetic_op_to_string(c.operation);
        } else if constexpr (std::is_same_v<T, PushCommand>) {
            return std::string("push ") + segment_to_string(c.segment) +
                   " " + std::to_string(c.index);
        } else if constexpr (std::is_same_v<T, PopCommand>) {
            return std::string("pop ") + segment_to_string(c.segment) +
                   " " + std::to_string(c.index);
        } else if constexpr (std::is_same_v<T, LabelCommand>) {
            return "label " + c.label_name;
        } else if constexpr (std::is_same_v<T, GotoCommand>) {
            return "goto " + c.label_name;
        } else if constexpr (std::is_same_v<T, IfGotoCommand>) {
            return "if-goto " + c.label_name;
        } else if constexpr (std::is_same_v<T, FunctionCommand>) {
            return "function " + c.function_name + " " +
                   std::to_string(c.num_locals);
        } else if constexpr (std::is_same_v<T, CallCommand>) {
            return "call " + c.function_name + " " +
                   std::to_string(c.num_args);
        } else if constexpr (std::is_same_v<T, ReturnCommand>) {
            return "return";
        }
    }, cmd);
}

bool is_branching_command(const VMCommand& cmd) {
    return std::holds_alternative<GotoCommand>(cmd) ||
           std::holds_alternative<IfGotoCommand>(cmd) ||
           std::holds_alternative<CallCommand>(cmd) ||
           std::holds_alternative<ReturnCommand>(cmd);
}

bool modifies_stack(const VMCommand& cmd) {
    // Label is a no-op; everything else affects the stack or control flow
    return !std::holds_alternative<LabelCommand>(cmd);
}

}  // namespace n2t
