cmake_minimum_required(VERSION 3.24)

option(BUILD_BENCHMARKS "Build benchmark programs" ON)
option(BUILD_CLI "Build command-line interface" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_PYTHON_BINDINGS)
    list(APPEND VCPKG_MANIFEST_FEATURES "python")
endif()

if(BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

# BLAS backend. Jizai ships under MIT and wants a permissively licensed dependency closure,
# so the default is OpenBLAS (BSD-3-Clause), or Accelerate on macOS, rather than Intel MKL
# (proprietary), which is not supported here. Any other FindBLAS vendor can be selected
# through BLA_VENDOR.
if(NOT DEFINED BLA_VENDOR)
    if(CMAKE_HOST_APPLE)
        set(BLA_VENDOR "Apple")
    else()
        set(BLA_VENDOR "OpenBLAS")
    endif()
endif()

if(BLA_VENDOR STREQUAL "Intel10_64lp_seq")
    message(FATAL_ERROR "Intel MKL is not supported; use OpenBLAS")
elseif(BLA_VENDOR STREQUAL "Arm")
    list(APPEND VCPKG_MANIFEST_FEATURES "armpl")
elseif(BLA_VENDOR STREQUAL "OpenBLAS")
    list(APPEND VCPKG_MANIFEST_FEATURES "openblas")
endif()

project(jizai CXX)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(jizai_target_contents)

# System Boost packages only install CMake configs for the compiled libraries, so the
# header-only ones fall back to Boost::headers
find_package(Boost CONFIG REQUIRED COMPONENTS container filesystem program_options)
foreach(BOOST_LIB algorithm any container_hash iterator math range unordered)
    if(NOT TARGET Boost::${BOOST_LIB})
        add_library(Boost::${BOOST_LIB} ALIAS Boost::headers)
    endif()
endforeach()
find_package(Ceres CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(FastFloat CONFIG REQUIRED)
find_package(nanoflann CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
find_package(xsimd CONFIG REQUIRED)
find_package(xtensor CONFIG REQUIRED)
find_package(xtl CONFIG REQUIRED)
find_path(POCKETFFT_INCLUDE_DIR pocketfft_hdronly.h REQUIRED
    HINTS ${CMAKE_SOURCE_DIR}/third_party/pocketfft
)
find_path(XTENSOR_BLAS_INCLUDE_DIR xtensor-blas/xlinalg.hpp REQUIRED)

if(MSVC)
    string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" MSVC_ARCH)
    file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" MSVC_REDIST_DIR)
    file(GLOB JIZAI_DLLS
        "${MSVC_REDIST_DIR}/debug_nonredist/${MSVC_ARCH}/Microsoft.VC*.OpenMP.LLVM/libomp140.*.dll"
    )
    if(NOT JIZAI_DLLS)
        message(FATAL_ERROR "LLVM OpenMP runtime not found in VCToolsRedistDir.")
    endif()
endif()

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
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
    add_subdirectory(test)
endif()
