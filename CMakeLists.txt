cmake_minimum_required(VERSION 3.16)
project(minibash VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -fsanitize=address,undefined")

include_directories(include)

# Check for readline
find_library(READLINE_LIB readline)
if(NOT READLINE_LIB)
  message(FATAL_ERROR "libreadline not found. Install: sudo apt-get install libreadline-dev")
endif()

set(SOURCES
    src/lexer.cpp
    src/parser.cpp
    src/environment.cpp
    src/history.cpp
    src/jobs.cpp
    src/builtins.cpp
    src/executor.cpp
    src/shell.cpp
)

# Main binary
add_executable(minibash src/main.cpp ${SOURCES})
target_link_libraries(minibash readline)

# Tests (no readline dependency needed)
add_executable(run_tests tests/test_main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/environment.cpp
    src/history.cpp
)

enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

install(TARGETS minibash DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "readline: ${READLINE_LIB}")
