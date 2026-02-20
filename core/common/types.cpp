// ==============================================================================
// Common Type Implementations
// ==============================================================================

#include "types.hpp"

namespace n2t {

Word bus_to_word(const Bus& bus) {
    Word result = 0;
    for (size_t i = 0; i < bus.size() && i < 16; i++) {
        if (bus[i] == Signal::HIGH) {
            result |= static_cast<Word>(1 << (bus.size() - 1 - i));
        }
    }
    return result;
}

Bus word_to_bus(Word word, size_t width) {
    Bus bus(width, Signal::LOW);
    for (size_t i = 0; i < width; i++) {
        if ((word >> (width - 1 - i)) & 1) {
            bus[i] = Signal::HIGH;
        }
    }
    return bus;
}

const char* segment_to_string(SegmentType segment) {
    switch (segment) {
        case SegmentType::LOCAL:    return "local";
        case SegmentType::ARGUMENT: return "argument";
        case SegmentType::THIS:     return "this";
        case SegmentType::THAT:     return "that";
        case SegmentType::CONSTANT: return "constant";
        case SegmentType::STATIC:   return "static";
        case SegmentType::TEMP:     return "temp";
        case SegmentType::POINTER:  return "pointer";
        default:                    return "unknown";
    }
}

const char* arithmetic_op_to_string(ArithmeticOp op) {
    switch (op) {
        case ArithmeticOp::ADD: return "add";
        case ArithmeticOp::SUB: return "sub";
        case ArithmeticOp::NEG: return "neg";
        case ArithmeticOp::EQ:  return "eq";
        case ArithmeticOp::GT:  return "gt";
        case ArithmeticOp::LT:  return "lt";
        case ArithmeticOp::AND: return "and";
        case ArithmeticOp::OR:  return "or";
        case ArithmeticOp::NOT: return "not";
        default:                return "unknown";
    }
}

}  // namespace n2t
