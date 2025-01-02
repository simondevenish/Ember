cmake_minimum_required(VERSION 3.10)
project(Ember C CXX)

# C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Let’s define the path to the "include" folder
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add common compiler flags
add_compile_options(-Wall -Wextra -g -O0)

# Gather all .c source files from src/ (except main.c if it’s included there)
file(GLOB EMBER_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
     # Exclude main.c or emberpm.c if you don’t want them built into the library
     # e.g.: "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" EXCLUDE main.c emberpm.c
)

# Build a static library named "Ember" from those sources
add_library(Ember STATIC ${EMBER_SOURCES})

# --- Build emberc CLI ---
# If main.c is outside of src/ or is called something else, adjust the path
add_executable(emberc "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
target_link_libraries(emberc PRIVATE Ember m pthread)

# --- Build emberpm tool ---
# Suppose 'emberpm.c' is in 'src/' folder
add_executable(emberpm "${CMAKE_CURRENT_SOURCE_DIR}/src/emberpm.c")
target_link_libraries(emberpm PRIVATE Ember m pthread)

# --------------------------
# Installation
# --------------------------

install(
    TARGETS Ember emberc emberpm
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

# --------------------------
# Tests (Optional)
# --------------------------
# Example:
# add_executable(ember_tests tests/test_something.cpp)
# target_link_libraries(ember_tests PRIVATE Ember gtest pthread)
# add_test(NAME EmberTests COMMAND ember_tests)
