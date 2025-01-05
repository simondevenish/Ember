cmake_minimum_required(VERSION 3.10)
project(Ember C CXX)

# ----------------------------------------------------------------------------
# 1) Language and Build Config
# ----------------------------------------------------------------------------
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

# ----------------------------------------------------------------------------
# 2) Optionally configure/build SDL2 from a submodule
# ----------------------------------------------------------------------------
# You can comment/uncomment or tailor these if you only want static linking:
#set(SDL_SHARED OFF CACHE BOOL "" FORCE)
#set(SDL_STATIC ON  CACHE BOOL "" FORCE)

# The official SDL2 repo has its own CMakeLists. We add it:
add_subdirectory(thirdparty/SDL2)

# ----------------------------------------------------------------------------
# 3) Gather Sources & Build Ember Library
# ----------------------------------------------------------------------------
file(GLOB EMBER_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
     # Exclude main.c or emberpm.c if you don’t want them built into the library
     # e.g.: "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" EXCLUDE main.c emberpm.c
)

# Build a static library named "Ember" from those sources
add_library(Ember STATIC ${EMBER_SOURCES})

# ----------------------------------------------------------------------------
# 4) Build emberc CLI
# ----------------------------------------------------------------------------
# If main.c is outside of src/ or named differently, adjust the path below
add_executable(emberc "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
target_link_libraries(emberc PRIVATE Ember m pthread)

# ----------------------------------------------------------------------------
# 5) Build emberpm Tool
# ----------------------------------------------------------------------------
# Suppose 'emberpm.c' is in 'src/' folder
add_executable(emberpm "${CMAKE_CURRENT_SOURCE_DIR}/src/emberpm.c")
target_link_libraries(emberpm PRIVATE Ember m pthread)

# ----------------------------------------------------------------------------
# 6) Link SDL2 with Ember or your executables if needed
# ----------------------------------------------------------------------------
# If your Ember library (or code within it) uses SDL2 calls:
target_link_libraries(Ember PRIVATE SDL2::SDL2)

# Or if you only need SDL2 in your executables (emberc, emberpm), do:
# target_link_libraries(emberc PRIVATE SDL2::SDL2)
# target_link_libraries(emberpm PRIVATE SDL2::SDL2)

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
