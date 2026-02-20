# ==============================================================================
# VM Emulator Library
# ==============================================================================

add_library(vm_engine STATIC
    vm_command.cpp
    vm_parser.cpp
    vm_memory.cpp
    vm_engine.cpp
)

target_link_libraries(vm_engine PUBLIC n2t_common)

target_include_directories(vm_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
