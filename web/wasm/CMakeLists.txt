# ==============================================================================
# WebAssembly Bindings — Emscripten + Embind
# ==============================================================================
# Builds all four engines into a single WASM module (n2t.js + n2t.wasm).
#
# Usage:
#   mkdir build-wasm && cd build-wasm
#   emcmake cmake .. -DBUILD_WEB=ON -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
#   emmake make -j$(nproc)
#
# Output lands in bin/n2t.{js,wasm}
# ==============================================================================

add_executable(n2t bindings.cpp)

target_link_libraries(n2t PRIVATE
    n2t_common
    cpu_engine
    vm_engine
    jack_debugger
    hdl_engine
)

# Emscripten-specific link flags
target_link_options(n2t PRIVATE
    # Embind glue
    --bind

    # ES6 module output so Vite can import it
    -sEXPORT_ES6=1
    -sMODULARIZE=1
    -sEXPORT_NAME=createN2TModule

    # Memory settings (64 MB initial, grows to 256 MB)
    -sINITIAL_MEMORY=67108864
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=268435456

    # We don't use the virtual filesystem
    -sNO_FILESYSTEM=1

    # Keep WASM small
    -sASSERTIONS=0
    -sDISABLE_EXCEPTION_CATCHING=0   # we use exceptions

    # Environment: web only
    -sENVIRONMENT=web,worker
)

# Place output next to other binaries
set_target_properties(n2t PROPERTIES
    OUTPUT_NAME "n2t"
    SUFFIX ".js"
)
