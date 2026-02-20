# ==============================================================================
# CPU Simulator Library
# ==============================================================================

add_library(cpu_engine STATIC
    instruction.cpp
    memory.cpp
    cpu.cpp
)

target_link_libraries(cpu_engine PUBLIC n2t_common)

target_include_directories(cpu_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
