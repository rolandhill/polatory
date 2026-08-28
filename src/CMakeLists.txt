set(TARGET polatory)

add_library(${TARGET} STATIC
    fmm/impl/biharmonic2d.cpp
    fmm/impl/biharmonic3d.cpp
    fmm/impl/cov_cubic.cpp
    fmm/impl/cov_exponential.cpp
    fmm/impl/cov_gaussian.cpp
    fmm/impl/cov_generalized_cauchy3.cpp
    fmm/impl/cov_generalized_cauchy5.cpp
    fmm/impl/cov_generalized_cauchy7.cpp
    fmm/impl/cov_generalized_cauchy9.cpp
    fmm/impl/cov_spherical.cpp
    fmm/impl/cov_spheroidal3.cpp
    fmm/impl/cov_spheroidal3_direct_part.cpp
    fmm/impl/cov_spheroidal3_fast_part.cpp
    fmm/impl/cov_spheroidal5.cpp
    fmm/impl/cov_spheroidal5_direct_part.cpp
    fmm/impl/cov_spheroidal5_fast_part.cpp
    fmm/impl/cov_spheroidal7.cpp
    fmm/impl/cov_spheroidal7_direct_part.cpp
    fmm/impl/cov_spheroidal7_fast_part.cpp
    fmm/impl/cov_spheroidal9.cpp
    fmm/impl/cov_spheroidal9_direct_part.cpp
    fmm/impl/cov_spheroidal9_fast_part.cpp
    fmm/impl/triharmonic2d.cpp
    fmm/impl/triharmonic3d.cpp
    fmm/make_fmm_evaluator.cpp
    isosurface/cluster.cpp
    isosurface/mesh_defects_finder.cpp
    isosurface/refine.cpp
    isosurface/snap.cpp
    kriging/variogram_calculator.cpp
    krylov/fgmres.cpp
    krylov/gmres_base.cpp
    krylov/gmres.cpp
    krylov/minres.cpp
    point_cloud/kdtree.cpp
    point_cloud/normal_estimator.cpp
    point_cloud/plane_estimator.cpp
    point_cloud/random_points.cpp
    point_cloud/sdf_data_generator.cpp
)

if(UNIX)
    target_compile_options(${TARGET} PUBLIC -fPIC -Wall -Wextra -Werror)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC 15 reports a false positive when it inlines the std::sort in ScalFMM's M2L
        # interaction list (scalfmm/tree/utils.hpp): it cannot prove the runtime counter stays
        # within the fixed-size permutation array. Keep it a warning rather than an error.
        target_compile_options(${TARGET} PRIVATE -Wno-error=array-bounds)
    endif()
    # target_compile_options(${TARGET} PUBLIC -g -fno-omit-frame-pointer -fsanitize=address)
    # target_compile_options(${TARGET} PUBLIC -g -fno-omit-frame-pointer -fsanitize=undefined)
elseif(MSVC)
    target_compile_options(${TARGET} PUBLIC /W4 /WX /wd4702)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${TARGET} PUBLIC
            -Wno-unused-command-line-argument
        )
    endif()
endif()

target_compile_definitions(${TARGET} PUBLIC
    -DEIGEN_DONT_PARALLELIZE
    -DEIGEN_MPL2_ONLY
)
if(MSVC)
    target_compile_definitions(${TARGET} PUBLIC
        -DWIN32_LEAN_AND_MEAN
        -DNOMINMAX
    )
endif()

target_include_directories(${TARGET} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(${TARGET} SYSTEM PUBLIC
    ${CMAKE_BINARY_DIR}/scalfmm/install/include
)

target_link_libraries(${TARGET} PUBLIC
    Boost::boost
    Boost::filesystem
    ceres
    Eigen3::Eigen
    FastFloat::fast_float
    nanoflann::nanoflann
    OpenMP::OpenMP_CXX
)

include(ExternalProject)

set(SCALFMM_CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/scalfmm/install
    -Dscalfmm_BUILD_CHECK=OFF
    -Dscalfmm_BUILD_EXAMPLES=OFF
    -Dscalfmm_BUILD_TOOLS=OFF
)

if(MSVC)
    list(APPEND SCALFMM_CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=clang++
        -DCMAKE_CXX_FLAGS=-fopenmp
    )
endif()

if(USE_MKL)
    target_compile_definitions(${TARGET} PUBLIC
        # -DEIGEN_USE_MKL_ALL
        -DEIGEN_USE_BLAS
    )

    # MKL's FFTW wrapper headers: under vcpkg they land in the installed tree,
    # under a system oneAPI install they sit next to the other MKL headers.
    if(MKL_ROOT)
        set(MKL_FFTW_INCLUDE_DIR "${MKL_ROOT}/include/fftw")
    else()
        set(MKL_FFTW_INCLUDE_DIR "${VCPKG_DIR}/include/fftw")
    endif()

    target_include_directories(${TARGET} PUBLIC
        ${MKL_FFTW_INCLUDE_DIR}
    )

    target_link_libraries(${TARGET} PUBLIC
        $<LINK_ONLY:MKL::MKL>
    )

    list(APPEND SCALFMM_CMAKE_ARGS
        -Dscalfmm_USE_MKL=ON
    )
elseif(APPLE)
    target_compile_definitions(${TARGET} PUBLIC
        -DEIGEN_USE_BLAS
    )

    target_link_libraries(${TARGET} INTERFACE
        "-framework Accelerate"
        FFTW3::fftw3
    )

    list(APPEND SCALFMM_CMAKE_ARGS
        -DBLA_VENDOR=Apple
    )
endif()

if(APPLE)
    # Avoid linker error with XCode 15
    target_link_options(${TARGET} PUBLIC -ld_classic)
endif()

externalproject_add(
    scalfmm
    PREFIX ${CMAKE_BINARY_DIR}/scalfmm
    GIT_REPOSITORY https://github.com/polatory/ScalFMM3.git
    GIT_TAG polatory
    GIT_SHALLOW ON
    GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    PATCH_COMMAND ${CMAKE_COMMAND} -DSCALFMM_SOURCE_DIR=<SOURCE_DIR>
                  -P ${CMAKE_SOURCE_DIR}/cmake/patch_scalfmm.cmake
    CMAKE_ARGS ${SCALFMM_CMAKE_ARGS}
)

add_dependencies(polatory scalfmm)

if(UNIX)
    # target_link_options(${TARGET} PUBLIC -fsanitize=address)
    # target_link_options(${TARGET} PUBLIC -fsanitize=undefined)
endif()
