# ==============================================================================
# Common Utilities Library
# ==============================================================================
# This library contains shared code used by all components:
# - Type definitions (Word, Address, SegmentType, etc.)
# - Error handling classes
# - Base parser utilities
# ==============================================================================

# Create a library for common utilities
# This is an INTERFACE library because it's header-only (no .cpp files yet)
add_library(n2t_common INTERFACE)

# Make headers available to targets that link with this library
target_include_directories(n2t_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Require C++17
target_compile_features(n2t_common INTERFACE cxx_std_17)

# When we add .cpp implementations later, we'll change this to:
# add_library(n2t_common
#     types.cpp
#     error.cpp
# )
