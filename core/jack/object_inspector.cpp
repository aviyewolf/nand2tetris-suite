// ==============================================================================
// Jack Object Inspector - Implementation
// ==============================================================================

#include "object_inspector.hpp"
#include "error.hpp"
#include <sstream>

namespace n2t {

ObjectInspector::ObjectInspector(const VMMemory& memory, const SourceMap& source_map)
    : memory_(memory)
    , source_map_(source_map)
{}

InspectedObject ObjectInspector::inspect_object(Address address,
                                                 const std::string& class_name) const {
    const ClassLayout* layout = source_map_.get_class_layout(class_name);
    if (!layout) {
        throw RuntimeError("Unknown class: '" + class_name + "'");
    }

    InspectedObject obj;
    obj.class_name = class_name;
    obj.heap_address = address;

    for (size_t i = 0; i < layout->fields.size(); i++) {
        const auto& field_def = layout->fields[i];
        Word raw = memory_.read_ram(
            static_cast<Address>(address + static_cast<Address>(i)));

        InspectedField field;
        field.field_name = field_def.name;
        field.type_name = field_def.type_name;
        field.raw_value = raw;
        field.signed_value = static_cast<int16_t>(raw);
        field.is_reference = !is_primitive_type(field_def.type_name);

        obj.fields.push_back(field);
    }

    return obj;
}

InspectedObject ObjectInspector::inspect_this(
    const std::string& current_function) const {
    // Get the THIS pointer (RAM[3])
    Address this_addr = memory_.read_ram(VMAddress::THIS);

    // Determine class name from function name
    auto dot_pos = current_function.find('.');
    if (dot_pos == std::string::npos) {
        throw RuntimeError("Cannot determine class from function: '"
                           + current_function + "'");
    }
    std::string class_name = current_function.substr(0, dot_pos);

    return inspect_object(this_addr, class_name);
}

InspectedArray ObjectInspector::inspect_array(Address address, size_t length) const {
    InspectedArray arr;
    arr.heap_address = address;
    arr.length = length;
    arr.elements.reserve(length);

    for (size_t i = 0; i < length; i++) {
        arr.elements.push_back(
            memory_.read_ram(static_cast<Address>(address + i)));
    }

    return arr;
}

std::string ObjectInspector::format_object(const InspectedObject& obj) {
    std::ostringstream oss;
    oss << obj.class_name << " @" << obj.heap_address << " {";

    for (size_t i = 0; i < obj.fields.size(); i++) {
        if (i > 0) oss << ",";
        const auto& f = obj.fields[i];
        oss << " " << f.field_name << ": ";
        if (f.is_reference) {
            oss << "@" << f.raw_value;
        } else {
            oss << f.signed_value;
        }
    }

    oss << " }";
    return oss.str();
}

std::string ObjectInspector::format_array(const InspectedArray& arr) {
    std::ostringstream oss;
    oss << "Array @" << arr.heap_address << " [";

    for (size_t i = 0; i < arr.elements.size(); i++) {
        if (i > 0) oss << ", ";
        oss << static_cast<int16_t>(arr.elements[i]);
    }

    oss << "]";
    return oss.str();
}

bool ObjectInspector::is_primitive_type(const std::string& type_name) {
    return type_name == "int" || type_name == "char" || type_name == "boolean";
}

}  // namespace n2t
