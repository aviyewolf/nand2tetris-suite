# HDL Simulator Library
# Parses HDL files, simulates combinational chips, runs .tst test scripts

add_library(hdl_engine STATIC
    hdl_parser.cpp
    hdl_chip.cpp
    hdl_builtins.cpp
    hdl_engine.cpp
    tst_runner.cpp
)

target_link_libraries(hdl_engine PUBLIC n2t_common)
target_include_directories(hdl_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
