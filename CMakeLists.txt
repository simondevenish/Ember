cmake_minimum_required(VERSION 3.10)
project(Ember C CXX)

# ----------------------------------------------------------------------------
# 1) Language and Build Config
# ----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4" CACHE STRING "Minimum OS X deployment version")

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ----------------------------------------------------------------------------
# Include Directories
# ----------------------------------------------------------------------------
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add common compiler flags
add_compile_options(-Wall -Wextra -g -O0)

# ----------------------------------------------------------------------------
# 2) Configure/Build Raylib from submodule (optional configuration)
# ----------------------------------------------------------------------------
# If you want raylib as static (not shared), you can force these:
# set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# Disable examples/tests (just as an example):
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Add the subdirectory for raylib
add_subdirectory(thirdparty/raylib)

# ----------------------------------------------------------------------------
# 3) Gather Sources & Build Ember Library
# ----------------------------------------------------------------------------
file(GLOB EMBER_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Build a static library named "Ember" from those sources
add_library(Ember STATIC ${EMBER_SOURCES})

# ----------------------------------------------------------------------------
# 4) Build emberc CLI
# ----------------------------------------------------------------------------
add_executable(emberc "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
target_link_libraries(emberc PRIVATE Ember m pthread)

# ----------------------------------------------------------------------------
# 5) Build emberpm Tool
# ----------------------------------------------------------------------------
add_executable(emberpm "${CMAKE_CURRENT_SOURCE_DIR}/src/emberpm.c")
target_link_libraries(emberpm PRIVATE Ember m pthread)

# ----------------------------------------------------------------------------
# 6) Link Raylib with Ember
# ----------------------------------------------------------------------------
# The 'raylib' target is provided by the raylib subdirectory's CMakeLists.
target_link_libraries(Ember PRIVATE raylib)

target_link_libraries(emberc PRIVATE raylib)
target_link_libraries(emberpm PRIVATE raylib)

# ----------------------------------------------------------------------------
# 7) Installation
# ----------------------------------------------------------------------------
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

# ----------------------------------------------------------------------------
# 8) (Optional) Tests
# ----------------------------------------------------------------------------
# Example:
# add_executable(ember_tests tests/test_something.cpp)
# target_link_libraries(ember_tests PRIVATE Ember gtest pthread)
# add_test(NAME EmberTests COMMAND ember_tests)
