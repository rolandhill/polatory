cmake_minimum_required(VERSION 3.18)

option(BUILD_BENCHMARKS "Build benchmark programs" ON)
option(BUILD_CLI "Build command-line interface" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_PYTHON_BINDINGS)
    list(APPEND VCPKG_MANIFEST_FEATURES "python")
endif()

project(Polatory CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(polatory_target_contents)

# BLAS backend. Polatory ships under MIT and wants a permissively licensed dependency closure,
# so the default is OpenBLAS (BSD-3-Clause) rather than Intel MKL (proprietary). Selecting it
# through BLA_VENDOR keeps FindBLAS portable across system packages, Homebrew and vcpkg, and the
# same vendor is handed down to the ScalFMM sub-build; see src/CMakeLists.txt.
if(NOT DEFINED BLA_VENDOR)
    if(APPLE)
        set(BLA_VENDOR "Apple")
    else()
        set(BLA_VENDOR "OpenBLAS")
    endif()
endif()

find_package(BLAS REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem program_options)
find_package(Ceres CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(FastFloat CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(nanoflann CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

add_subdirectory(src)

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

if(BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_PYTHON_BINDINGS)
    find_package(pybind11 CONFIG REQUIRED)
    add_subdirectory(python)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
