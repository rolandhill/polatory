// --------------------------------
// See LICENCE file at project root
// File : scalfmm/utils/fftw.hpp
// --------------------------------
//
// Polatory modification of a CeCILL-C licensed ScalFMM3 file.
//
// Upstream backs scalfmm::fftw::fft on FFTW3, whose only implementations are FFTW itself
// (GPL-2.0-or-later) and Intel MKL's FFTW-compatible interface (proprietary). Neither can be
// shipped as part of a permissively licensed distribution, so the transforms here run on
// PocketFFT (BSD-3-Clause) instead. The public interface of scalfmm::fftw::fft is unchanged,
// so no ScalFMM caller is affected.
//
// This file is installed over include/scalfmm/utils/fftw.hpp by cmake/patch_scalfmm.cmake in
// the polatory tree. As a modification of a CeCILL-C file it is published under CeCILL-C.
//
#ifndef SCALFMM_UTILS_FFTW_HPP
#define SCALFMM_UTILS_FFTW_HPP

#include "scalfmm/utils/massert.hpp"

#include "xtensor/containers/xarray.hpp"
#include "xtensor/containers/xcontainer.hpp"
#include "xtensor/core/xsemantic.hpp"
#include "xtensor/containers/xstorage.hpp"
#include "xtensor/core/xtensor_forward.hpp"
#include "xtl/xcomplex.hpp"

// PocketFFT rebuilds its twiddle tables on every call unless a plan cache is enabled, which
// would lose the reuse FFTW gave us through its plan objects. ScalFMM only ever transforms a
// handful of distinct lengths (2 * order - 1, for the few orders held in the interpolator
// cache), so a small LRU is enough. The cache is mutex-guarded inside PocketFFT and therefore
// safe to share between the per-thread fft handlers.
#ifndef POCKETFFT_CACHE_SIZE
#define POCKETFFT_CACHE_SIZE 16
#endif
// Every transform below is issued with nthreads == 1: ScalFMM already calls into this from
// inside an OpenMP parallel region, one fft handler per thread.
#ifndef POCKETFFT_NO_MULTITHREADING
#define POCKETFFT_NO_MULTITHREADING
#endif
// GCC reports a false positive in multi_iter: the p_i / p_o offset arrays are filled by
// advance(n), which every read through iofs() / oofs() is preceded by, but once general_r2c's
// loop is inlined and vectorised GCC can no longer prove the ordering. Scoped to PocketFFT so
// that polatory's own code keeps the warning as an error. Being reached through -isystem is not
// enough here: the diagnostic is raised after inlining.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "scalfmm/utils/pocketfft_hdronly.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <complex>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <vector>

// Define if the library is going to be using exceptions.
#if (!defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND))
#undef XTENSOR_FFTW_DISABLE_EXCEPTIONS
#define XTENSOR_FFTW_DISABLE_EXCEPTIONS
#endif

// Exception support.
#if defined(XTENSOR_FFTW_DISABLE_EXCEPTIONS)
#include <iostream>
#define XTENSOR_FFTW_THROW(_, msg)       \
    {                                    \
      std::cerr << msg << std::endl;     \
      std::abort();                      \
    }
#else
#define XTENSOR_FFTW_THROW(exception, msg) throw exception(msg)
#endif

namespace  xt::fftw
{
          // output to DFT-dimensions conversion
        template<typename output_t>
        inline auto dft_dimensions_from_output(const xt::xarray<output_t, xt::layout_type::row_major>& output,
                                               bool half_plus_one_out, bool odd_last_dim = false)
        {
            auto dft_dimensions = output.shape();

            if(half_plus_one_out)
            {   // r2c
                auto n = dft_dimensions.size();
                if(!odd_last_dim)
                {
                    dft_dimensions[n - 1] = (dft_dimensions[n - 1] - 1) * 2;
                }
                else
                {
                    dft_dimensions[n - 1] = (dft_dimensions[n - 1] - 1) * 2 + 1;
                }
            }

            return dft_dimensions;
        }
}

namespace scalfmm::fftw
{
    /**
     * @brief Real-to-complex FFT handler backed by PocketFFT.
     *
     * Holds the pair of buffers a multi-dimensional real transform of a fixed shape works on,
     * along with the geometry PocketFFT needs to describe them. Unlike FFTW there is no plan
     * object to own: create_plan() and create_inverse_plan() only record the shape, strides and
     * normalisation factor, and destroy_plan() / destroy_inverse_plan() are no-ops kept so the
     * upstream interface is preserved.
     *
     * @tparam ValueType float or double
     * @tparam dim rank of the transform
     */
    template<typename ValueType, std::size_t dim>
    struct fft
    {
        using value_type = ValueType;
        using fft_type = xt::xarray<value_type>;
        using transformed_fft_type = xt::xarray<std::complex<value_type>>;

        /**
         * @brief Construct a new fft object
         *
         */
        fft() = default;

        /**
         * @brief Construct a new fft object
         *
         */
        fft(fft const&) = default;

        /**
         * @brief Construct a new fft object
         *
         */
        fft(fft&&) = delete;

        /**
         * @brief
         *
         * @return fft&
         */
        inline auto operator=(fft const&) -> fft& = default;

        /**
         * @brief
         *
         * @return fft&
         */
        inline auto operator=(fft&&) noexcept -> fft& = delete;

        /**
         * @brief Size the buffers for an interpolation order and set up both transforms.
         *
         * The real buffer is a dim-cube of side 2 * order - 1; since that is odd, the
         * half-complex last dimension is (2 * order - 1) / 2 + 1 == order.
         *
         * @param order
         */
        auto initialize(std::size_t order) -> void
        {
            std::vector<std::size_t> input_forward_shape(dim, (2 * order - 1));
            std::vector<std::size_t> output_forward_shape(dim, (2 * order - 1));
            output_forward_shape.at(dim - 1) = order;
            m_real_buffer.resize(input_forward_shape);
            m_complex_buffer.resize(output_forward_shape);
            create_plan();
            create_inverse_plan(true);
        }

        /**
         * @brief Create a plan object
         *
         */
        inline auto create_plan() -> void
        {
            if(!plan_exists)
            {
                setup_geometry();
                plan_exists = true;
            }
        }

        /**
         * @brief Create a inverse plan object
         *
         * @param odd_last_dim unused; upstream passed it to dft_dimensions_from_output with
         *                     half_plus_one_out == false, where it was ignored.
         */
        inline auto create_inverse_plan([[maybe_unused]] bool odd_last_dim = false) -> void
        {
            if(!plan_inv_exists)
            {
                setup_geometry();
                plan_inv_exists = true;
            }
        }

        /**
         * @brief
         *
         * @param input
         * @param output
         */
        inline auto execute_plan(fft_type const& input, transformed_fft_type& output) -> void
        {
            execute_plan(input);
            assertm(m_complex_buffer.size() == output.size(),
                    "Output complex buffer does not have the same size as the fft handler buffer!");
            std::copy(m_complex_buffer.begin(), m_complex_buffer.end(), output.begin());
        }

        /**
         * @brief
         *
         * @param input
         */
        inline auto execute_plan(fft_type const& input) -> void
        {
            check_geometry(plan_exists);
            assertm(input.size() == m_real_buffer.size(),
                    "Input real buffer does not have the same size as the fft handler buffer!");
            std::copy(input.begin(), input.end(), m_real_buffer.begin());
            pocketfft::r2c(m_shape, m_stride_real, m_stride_complex, m_axes, pocketfft::FORWARD,
                           m_real_buffer.data(), m_complex_buffer.data(), value_type(1));
        }

        /**
         * @brief
         *
         * @param input
         * @param output
         */
        inline auto execute_inverse_plan(transformed_fft_type const& input, fft_type& output) -> void
        {
            execute_inverse_plan(input);
            assertm(m_real_buffer.size() == output.size(),
                    "Output complex buffer does not have the same size as the fft handler buffer!");
            std::copy(m_real_buffer.begin(), m_real_buffer.end(), output.begin());
        }

        /**
         * @brief
         *
         * @param input
         */
        inline auto execute_inverse_plan(transformed_fft_type const& input) -> void
        {
            check_geometry(plan_inv_exists);
            assertm(input.size() == m_complex_buffer.size(),
                    "Input real buffer does not have the same size as the fft handler buffer!");
            std::copy(input.begin(), input.end(), m_complex_buffer.begin());
            // The 1 / N_dft that upstream applied as a separate pass over the buffer is folded
            // into PocketFFT's scale factor.
            pocketfft::c2r(m_shape, m_stride_complex, m_stride_real, m_axes, pocketfft::BACKWARD,
                           m_complex_buffer.data(), m_real_buffer.data(), value_type(1) / N_dft);
        }

        /**
         * @brief No-op; PocketFFT owns its plans through an internal cache.
         *
         */
        inline auto destroy_plan() -> void { plan_exists = false; }

        /**
         * @brief No-op; PocketFFT owns its plans through an internal cache.
         *
         */
        inline auto destroy_inverse_plan() -> void { plan_inv_exists = false; }

        /**
         * @brief
         *
         * @return fft_type const&
         */
        [[nodiscard]] inline auto real_buffer() const -> fft_type const& { return m_real_buffer; }

        /**
         * @brief
         *
         * @return fft_type const&
         */
        [[nodiscard]] inline auto creal_buffer() const -> fft_type const& { return m_real_buffer; }

        /**
         * @brief
         *
         * @return fft_type&
         */
        [[nodiscard]] inline auto real_buffer() -> fft_type& { return m_real_buffer; }

        /**
         * @brief
         *
         * @return transformed_fft_type const&
         */
        [[nodiscard]] inline auto complex_buffer() const -> transformed_fft_type const& { return m_complex_buffer; }

        /**
         * @brief
         *
         * @return transformed_fft_type const&
         */
        [[nodiscard]] inline auto ccomplex_buffer() const -> transformed_fft_type const& { return m_complex_buffer; }

        /**
         * @brief
         *
         * @return transformed_fft_type&
         */
        [[nodiscard]] inline auto complex_buffer() -> transformed_fft_type& { return m_complex_buffer; }

        /**
         * @brief Destroy the fft object
         *
         */
        ~fft()
        {
            destroy_plan();
            destroy_inverse_plan();
        }

        /**
         * @brief
         *
         */
        fft_type m_real_buffer{};

        /**
         * @brief
         *
         */
        transformed_fft_type m_complex_buffer{};

        /**
         * @brief
         *
         */
        value_type N_dft{1};

        /**
         * @brief
         *
         */
        bool plan_exists{false};

        /**
         * @brief
         *
         */
        bool plan_inv_exists{false};

      private:
        /**
         * @brief Record the transform geometry from the current buffer shapes.
         *
         * The transform runs over every axis, on row-major contiguous buffers, so the strides
         * are the trailing products of each shape in bytes. Both directions share the real
         * shape: PocketFFT takes the real shape in either case and derives the half-complex
         * last dimension itself, which is what makes the odd last dimension used here work
         * without the fix-ups upstream needed for FFTW.
         */
        inline auto setup_geometry() -> void
        {
            auto const& real_shape = m_real_buffer.shape();
            auto const& complex_shape = m_complex_buffer.shape();

            m_shape.assign(real_shape.begin(), real_shape.end());

            m_axes.resize(dim);
            std::iota(m_axes.begin(), m_axes.end(), std::size_t(0));

            m_stride_real = contiguous_strides(real_shape, sizeof(value_type));
            m_stride_complex = contiguous_strides(complex_shape, sizeof(std::complex<value_type>));

            N_dft = static_cast<value_type>(std::accumulate(m_shape.begin(), m_shape.end(), std::size_t(1),
                                                            std::multiplies<std::size_t>()));
        }

        /**
         * @brief Row-major byte strides for a shape.
         */
        template<typename Shape>
        static inline auto contiguous_strides(Shape const& shape, std::size_t element_size) -> pocketfft::stride_t
        {
            pocketfft::stride_t strides(shape.size());
            auto stride = static_cast<std::ptrdiff_t>(element_size);
            for(std::size_t i = shape.size(); i-- > 0;)
            {
                strides[i] = stride;
                stride *= static_cast<std::ptrdiff_t>(shape[i]);
            }
            return strides;
        }

        /**
         * @brief Stand-in for upstream's null-plan check.
         */
        inline auto check_geometry(bool exists) const -> void
        {
            if(!exists || m_shape.size() != dim)
            {
                XTENSOR_FFTW_THROW(std::runtime_error,
                                   "The fft handler has no transform geometry. initialize() must be called before "
                                   "a transform is executed.");
            }
        }

        pocketfft::shape_t m_shape{};
        pocketfft::shape_t m_axes{};
        pocketfft::stride_t m_stride_real{};
        pocketfft::stride_t m_stride_complex{};
    };
}   // namespace scalfmm::fftw

#endif   // SCALFMM_UTILS_FFTW_HPP
