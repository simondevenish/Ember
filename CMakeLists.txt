cmake_minimum_required(VERSION 3.10)
project(Ember C CXX)

# ----------------------------------------------------------------------------
# 1) Language and Build Config
# ----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)

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
# 2) Configure/Build SDL2 from submodule *statically*
# ----------------------------------------------------------------------------
# Force SDL2 to build as static (and not shared):
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

# The official SDL2 repo has its own CMakeLists. We add it:
add_subdirectory(thirdparty/SDL2)

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
# 6) Link SDL2 Statically with Ember
# ----------------------------------------------------------------------------
# Since we built SDL2 as a static lib, link with SDL2::SDL2-static.
# (The target name may differ depending on the SDL2 CMake config.)
target_link_libraries(Ember PRIVATE SDL2::SDL2-static)

# target_link_libraries(emberc PRIVATE SDL2::SDL2-static)
# target_link_libraries(emberpm PRIVATE SDL2::SDL2-static)

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
