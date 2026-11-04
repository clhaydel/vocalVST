// Minimal radix-2 complex FFT (preallocated, real-time safe after prepare()).
#pragma once
#include "Common.h"
#include <complex>

namespace hv
{
class Fft
{
public:
    void prepare (int sizePow2)
    {
        n = sizePow2;
        logN = 0;
        while ((1 << logN) < n) ++logN;
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0f, -kTwoPi * (float) i / (float) n);
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < logN; ++b)
                if (i & (1 << b)) r |= 1 << (logN - 1 - b);
            rev[(size_t) i] = r;
        }
        work.resize ((size_t) n);
    }

    int size() const noexcept { return n; }

    /** In-place forward transform of `data` (length n). */
    void forward (std::complex<float>* data) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (data[i], data[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto t = tw[(size_t) (j * step)] * data[i + j + half];
                    data[i + j + half] = data[i + j] - t;
                    data[i + j] += t;
                }
        }
    }

    /** Magnitude spectrum (n/2+1 bins) of a real frame. */
    void magnitude (const float* in, float* magOut) noexcept
    {
        for (int i = 0; i < n; ++i) work[(size_t) i] = { in[i], 0.0f };
        forward (work.data());
        for (int i = 0; i <= n / 2; ++i) magOut[i] = std::abs (work[(size_t) i]);
    }

private:
    int n = 0, logN = 0;
    std::vector<std::complex<float>> tw, work;
    std::vector<int> rev;
};
} // namespace hv
