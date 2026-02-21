# Test Suite
# Unit and integration tests for each component

# VM Engine tests
add_executable(vm_engine_test vm_engine_test.cpp)
target_link_libraries(vm_engine_test PRIVATE vm_engine)
add_test(NAME vm_engine_test COMMAND vm_engine_test)

# CPU Engine tests
add_executable(cpu_engine_test cpu_engine_test.cpp)
target_link_libraries(cpu_engine_test PRIVATE cpu_engine)
add_test(NAME cpu_engine_test COMMAND cpu_engine_test)

# Jack Debugger tests
add_executable(jack_debugger_test jack_debugger_test.cpp)
target_link_libraries(jack_debugger_test PRIVATE jack_debugger)
add_test(NAME jack_debugger_test COMMAND jack_debugger_test)

# HDL Engine tests
add_executable(hdl_engine_test hdl_engine_test.cpp)
target_link_libraries(hdl_engine_test PRIVATE hdl_engine)
add_test(NAME hdl_engine_test COMMAND hdl_engine_test)
