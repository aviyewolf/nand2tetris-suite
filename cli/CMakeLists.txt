# ==============================================================================
# Command-Line Tools
# ==============================================================================
# Four CLI executables for the Nand2Tetris simulation engines.
# Each supports batch mode (run and exit) and interactive REPL mode.
# ==============================================================================

# Shared line editor (command history, arrow keys)
add_library(line_editor STATIC line_editor.cpp)
target_include_directories(line_editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# HDL Simulator
add_executable(hdl_sim hdl_sim.cpp)
target_link_libraries(hdl_sim PRIVATE hdl_engine line_editor)

# CPU Simulator
add_executable(cpu_sim cpu_sim.cpp)
target_link_libraries(cpu_sim PRIVATE cpu_engine line_editor)

# VM Emulator
add_executable(vm_emu vm_emu.cpp)
target_link_libraries(vm_emu PRIVATE vm_engine line_editor)

# Jack Debugger
add_executable(jack_debug jack_debug.cpp)
target_link_libraries(jack_debug PRIVATE jack_debugger line_editor)
