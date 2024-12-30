cmake_minimum_required(VERSION 3.10)
project(Ember C CXX)

# C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Let’s define the path to the "include" folder; best to use an absolute or CMAKE_CURRENT_SOURCE_DIR
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add common compiler flags
add_compile_options(-Wall -Wextra -g -O0)

# Gather all .c source files from src/
file(GLOB EMBER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Build a static library named "Ember" from those sources
add_library(Ember STATIC ${EMBER_SOURCES})

# Build the emberc CLI from main.c (adjust path if main.c is in another folder)
add_executable(emberc "${CMAKE_CURRENT_SOURCE_DIR}/main.c")

# Link emberc to the Ember library plus the standard libs (math, pthread, etc.)
target_link_libraries(emberc PRIVATE Ember m pthread)

# --------------------------
# Installation
# --------------------------

# 1) Install the library + executable
#    - RUNTIME (exe) goes to bin
#    - ARCHIVE (static lib) goes to lib
#    - LIBRARY (shared lib, if you ever switch) also goes to lib
install(
    TARGETS Ember emberc
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# 2) Install headers
#    This copies everything under 'include/' to the system include path:
#    For example, if your headers are in include/Ember/*.h, 
#    they end up in /usr/local/include/Ember/*.h (by default).
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

# --------------------------
# (Optional) Tests
# --------------------------
# Example:
# add_executable(ember_tests tests/test_something.cpp)
# target_link_libraries(ember_tests PRIVATE Ember gtest pthread)
# add_test(NAME EmberTests COMMAND ember_tests)
