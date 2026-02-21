// ==============================================================================
// Jack Object Inspector
// ==============================================================================
// Reads heap objects and arrays using Jack type info from the source map.
// Provides human-readable formatting of object state for debugging.
// ==============================================================================

#ifndef NAND2TETRIS_JACK_OBJECT_INSPECTOR_HPP
#define NAND2TETRIS_JACK_OBJECT_INSPECTOR_HPP

#include "source_map.hpp"
#include "vm_memory.hpp"
#include <string>
#include <vector>

namespace n2t {

// ==============================================================================
// Inspected Types
// ==============================================================================

struct InspectedField {
    std::string field_name;
    std::string type_name;
    Word raw_value;
    int16_t signed_value;
    bool is_reference;  // true if type is a class (not int/char/boolean)
};

struct InspectedObject {
    std::string class_name;
    Address heap_address;
    std::vector<InspectedField> fields;
};

struct InspectedArray {
    Address heap_address;
    size_t length;
    std::vector<Word> elements;
};

// ==============================================================================
// Object Inspector Class
// ==============================================================================

class ObjectInspector {
public:
    ObjectInspector(const VMMemory& memory, const SourceMap& source_map);

    // Inspect an object at a heap address using the given class layout
    InspectedObject inspect_object(Address address,
                                   const std::string& class_name) const;

    // Inspect the current 'this' object (RAM[THIS] pointer)
    InspectedObject inspect_this(const std::string& current_function) const;

    // Inspect an array at a heap address with given length
    InspectedArray inspect_array(Address address, size_t length) const;

    // Format an inspected object for display
    static std::string format_object(const InspectedObject& obj);

    // Format an inspected array for display
    static std::string format_array(const InspectedArray& arr);

private:
    const VMMemory& memory_;
    const SourceMap& source_map_;

    static bool is_primitive_type(const std::string& type_name);
};

}  // namespace n2t

#endif  // NAND2TETRIS_JACK_OBJECT_INSPECTOR_HPP
