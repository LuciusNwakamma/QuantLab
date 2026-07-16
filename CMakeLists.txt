cmake_minimum_required(VERSION 3.20)
project(QuantLab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        "$<$<CONFIG:Debug>:-g;-O0;-fsanitize=address,undefined>"
        "$<$<CONFIG:Release>:-O3;-march=native;-DNDEBUG>"
    )
    add_link_options(
        "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
    )
endif()

# Options
option(QUANTLAB_BUILD_TESTS   "Build unit tests"      ON)
option(QUANTLAB_BUILD_BENCHMARKS "Build benchmarks"   OFF)
option(QUANTLAB_BUILD_PYTHON_BINDINGS "Build pybind11 Python bindings" ON)

# Find packages
find_package(Threads REQUIRED)

# Third-party dependencies via FetchContent
include(FetchContent)

# Google Test
if(QUANTLAB_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    include(GoogleTest)
endif()

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)

if(QUANTLAB_BUILD_PYTHON_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development QUIET)
    if(Python3_Development_FOUND)
        set(PYBIND11_FINDPYTHON ON)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v2.13.6
        )
        FetchContent_MakeAvailable(pybind11)
    else()
        message(WARNING "Python3 development headers were not found; disabling QUANTLAB_BUILD_PYTHON_BINDINGS. Install python3-dev in WSL to enable bindings.")
        set(QUANTLAB_BUILD_PYTHON_BINDINGS OFF)
    endif()
endif()

# Global include directories
include_directories(cpp/include)

# Subdirectories
add_subdirectory(cpp)
if(QUANTLAB_BUILD_TESTS)
    add_subdirectory(tests/cpp)
endif()
if(QUANTLAB_BUILD_PYTHON_BINDINGS)
    add_subdirectory(python_bindings)
endif()
