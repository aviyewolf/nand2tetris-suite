# ==============================================================================
# Common Utilities Library
# ==============================================================================
# This library contains shared code used by all components:
# - Type definitions (Word, Address, SegmentType, etc.)
# - Error handling classes
# - Base parser utilities
# ==============================================================================

# Create a library for common utilities
add_library(n2t_common STATIC
    types.cpp
)

# Make headers available to targets that link with this library
target_include_directories(n2t_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Require C++17
target_compile_features(n2t_common PUBLIC cxx_std_17)
