cmake_minimum_required(VERSION 3.10)
project(EmberScript C CXX)

# Set the C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Include directory for EmberScript headers
include_directories(include)

# Compiler warnings and other flags (example: -Wall -Wextra -g -O0)
add_compile_options(-Wall -Wextra -g -O0)

# Gather all source files from src/
file(GLOB EMBERSCRIPT_SOURCES src/*.c)

# Create static library for EmberScript
add_library(EmberScript STATIC ${EMBERSCRIPT_SOURCES})

# Optionally, you can install this library (if you want):
# install(TARGETS EmberScript DESTINATION lib)
# install(DIRECTORY include/ DESTINATION include)

# --- Build the executable for running scripts ---
# main.c can be anywhere; here we assume it's in the project root.
add_executable(ember_script_exec main.c)

# Link the executable against the EmberScript library
target_link_libraries(ember_script_exec PRIVATE EmberScript)

# If you'd like to install the executable:
# install(TARGETS ember_script_exec DESTINATION bin)
