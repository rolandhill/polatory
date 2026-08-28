# Patch applied to the ScalFMM3 sources by externalproject_add; see src/CMakeLists.txt.
#
# <scalfmm/tree/io.hpp> defines print_map and print_with_map as namespace-scope lambdas, so every
# translation unit that includes the header emits its own definition of each and linking
# libpolatory.a fails with "multiple definition of `scalfmm::io::print_map'". Marking them inline
# gives them the linkage a definition in a header needs.
#
# Idempotent: patched lines no longer match, so re-running this after a re-checkout is harmless.

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
