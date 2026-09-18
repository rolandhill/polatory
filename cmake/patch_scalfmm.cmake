# Patches applied to the ScalFMM3 sources by externalproject_add; see src/CMakeLists.txt.
#
# Expects SCALFMM_SOURCE_DIR (the checkout to patch) and POLATORY_SOURCE_DIR (this repository).
#
# Every step is idempotent, so re-running after a re-checkout is harmless: the regex patch no
# longer matches once applied, and the rest are file overwrites.

if(NOT SCALFMM_SOURCE_DIR OR NOT POLATORY_SOURCE_DIR)
    message(FATAL_ERROR "patch_scalfmm: SCALFMM_SOURCE_DIR and POLATORY_SOURCE_DIR must both be set")
endif()

# 1. <scalfmm/tree/io.hpp> defines print_map and print_with_map as namespace-scope lambdas, so every
# translation unit that includes the header emits its own definition of each and linking
# libpolatory.a fails with "multiple definition of `scalfmm::io::print_map'". Marking them inline
# gives them the linkage a definition in a header needs.

set(HEADER "${SCALFMM_SOURCE_DIR}/include/scalfmm/tree/io.hpp")

if(NOT EXISTS "${HEADER}")
    message(FATAL_ERROR "Cannot patch ScalFMM: ${HEADER} not found")
endif()

file(READ "${HEADER}" CONTENTS)

string(REGEX REPLACE "\n([ \t]*)auto (print_map|print_with_map) = \\[\\]"
                     "\n\\1inline auto \\2 = []" PATCHED "${CONTENTS}")

if(NOT PATCHED STREQUAL CONTENTS)
    file(WRITE "${HEADER}" "${PATCHED}")
    message(STATUS "Patched ${HEADER}: gave print_map and print_with_map inline linkage")
endif()

# 2. Swap the FFT backend from FFTW3 to PocketFFT.
#
# <scalfmm/utils/fftw.hpp> drives the M2L operator of the uniform interpolators through FFTW3,
# whose only implementations are FFTW itself (GPL-2.0-or-later) and Intel MKL's FFTW-compatible
# interface (proprietary). Polatory ships under MIT and wants a permissively licensed dependency
# closure, so the header is replaced with an interface-compatible one over PocketFFT
# (BSD-3-Clause). PocketFFT is copied in beside it so that it installs with the other ScalFMM
# headers and needs no include path of its own.

set(FFT_HEADERS
    "${POLATORY_SOURCE_DIR}/third_party/pocketfft/pocketfft_hdronly.h"
    "${POLATORY_SOURCE_DIR}/cmake/scalfmm/fftw.hpp"
)

foreach(SOURCE IN LISTS FFT_HEADERS)
    if(NOT EXISTS "${SOURCE}")
        message(FATAL_ERROR "Cannot patch ScalFMM: ${SOURCE} not found")
    endif()
    file(COPY "${SOURCE}" DESTINATION "${SCALFMM_SOURCE_DIR}/include/scalfmm/utils")
endforeach()

message(STATUS "Patched ${SCALFMM_SOURCE_DIR}/include/scalfmm/utils/fftw.hpp: FFT now runs on PocketFFT")

# 3. With FFTW gone, ScalFMM's own FFTW detection has nothing left to find.
#
# cmake/dependencies/fftw.cmake either takes MKL's FFTW interface or falls back to
# pkg_search_module(FFTW REQUIRED fftw3), so leaving it in place would reintroduce exactly the
# dependency step 2 removed. Polatory consumes ScalFMM as installed headers only -- it never links
# the ScalFMM CMake target -- so this file only has to let the sub-build configure.

file(WRITE "${SCALFMM_SOURCE_DIR}/cmake/dependencies/fftw.cmake"
"# Replaced by polatory's cmake/patch_scalfmm.cmake.
#
# The FFT is provided by PocketFFT, vendored into include/scalfmm/utils, so there is no external
# FFTW to discover. include() does not open a new scope, so FUSE_LIST is the caller's variable.
list(APPEND FUSE_LIST FFTW)
set(FFTW_FOUND TRUE)
")

message(STATUS "Patched ${SCALFMM_SOURCE_DIR}/cmake/dependencies/fftw.cmake: dropped the external FFTW requirement")
