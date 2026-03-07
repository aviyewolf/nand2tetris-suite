# Jack Debugger Library
add_library(jack_debugger STATIC
    source_map.cpp
    object_inspector.cpp
    jack_debugger.cpp
    jack_tokenizer.cpp
    jack_declaration_parser.cpp
    auto_source_map.cpp
)

target_link_libraries(jack_debugger PUBLIC vm_engine)

target_include_directories(jack_debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
