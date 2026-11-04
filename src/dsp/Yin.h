// YIN pitch tracker running on a decimated (~11-12 kHz) stream for CPU efficiency.
#pragma once
#include "Common.h"

namespace hv
{
class YinTracker
{
public:
    void prepare (double sampleRate)
    {
        sr     = sampleRate;
        decim  = std::max (1, (int) std::floor (sr / 11025.0));
        dsr    = sr / decim;
        win    = (int) std::lround (dsr * 0.025);
        minLag = std::max (2, (int) std::floor (dsr / 1000.0));
        maxLag = (int) std::ceil (dsr / 40.0);
        hop    = std::max (1, (int) std::lround (dsr * 0.005));
        ringLen = win + maxLag + 4;
        ring.assign ((size_t) ringLen, 0.0f);
        lin.assign ((size_t) ringLen, 0.0f);
        d.assign ((size_t) maxLag + 2, 0.0f);
        aa1.lowpass (sr, std::min (4000.0, dsr * 0.4), 0.5412);
        aa2.lowpass (sr, std::min (4000.0, dsr * 0.4), 1.3066);
        hpf.highpass (sr, 30.0, 0.7071);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        pos = phase = hopCount = 0;
        aa1.reset(); aa2.reset(); hpf.reset();
        f0 = 0.0f; conf = 0.0f;
        newFrame = false;
    }

    /** Push one full-rate sample; returns true when a new pitch estimate was produced. */
    inline bool push (float x) noexcept
    {
        const float y = aa2.process (aa1.process (hpf.process (x)));
        if (++phase < decim) return false;
        phase = 0;
        ring[(size_t) pos] = y;
        if (++pos >= ringLen) pos = 0;
        if (++hopCount < hop) return false;
        hopCount = 0;
        estimate();
        return true;
    }

    float frequency() const noexcept { return f0; }     // Hz, 0 when unvoiced
    float confidence() const noexcept { return conf; }  // 0..1 (1 - aperiodicity)
    int   decimation() const noexcept { return decim; }

private:
    void estimate() noexcept
    {
        // linearise the ring (oldest first)
        for (int i = 0; i < ringLen; ++i) lin[(size_t) i] = ring[(size_t) ((pos + i) % ringLen)];
        const float* x = lin.data() + (ringLen - win - maxLag);
        double energy = 0.0;
        for (int i = 0; i < win + maxLag; ++i) energy += (double) x[i] * x[i];
        if (energy < 1.0e-9 * (win + maxLag)) { f0 = 0.0f; conf = 0.0f; return; }

        // difference function + cumulative mean normalisation
        d[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double s = 0.0;
            for (int i = 0; i < win; ++i)
            {
                const float df = x[i] - x[i + tau];
                s += (double) df * df;
            }
            running += s;
            d[(size_t) tau] = running > 0.0 ? (float) (s * tau / running) : 1.0f;
        }
        int best = -1;
        constexpr float thr = 0.15f;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (d[(size_t) tau] < thr)
            {
                while (tau + 1 < maxLag && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {   // global minimum as a low-confidence fallback
            float mn = 10.0f;
            for (int tau = minLag; tau < maxLag; ++tau)
                if (d[(size_t) tau] < mn) { mn = d[(size_t) tau]; best = tau; }
        }
        if (best <= 0) { f0 = 0.0f; conf = 0.0f; return; }
        float refined = (float) best;
        if (best > 1 && best < maxLag)
        {
            const float y0 = d[(size_t) best - 1], y1 = d[(size_t) best], y2 = d[(size_t) best + 1];
            const float den = y0 - 2.0f * y1 + y2;
            if (std::abs (den) > 1.0e-9f) refined += clampf (0.5f * (y0 - y2) / den, -0.5f, 0.5f);
        }
        conf = clampf (1.0f - d[(size_t) best], 0.0f, 1.0f);
        f0   = (float) (dsr / refined);
        if (f0 < 40.0f || f0 > 1000.0f) { conf = 0.0f; }
    }

    double sr = 48000.0, dsr = 12000.0;
    int decim = 4, win = 300, minLag = 12, maxLag = 300, hop = 60, ringLen = 0;
    int pos = 0, phase = 0, hopCount = 0;
    std::vector<float> ring, lin, d;
    Biquad aa1, aa2, hpf;
    float f0 = 0.0f, conf = 0.0f;
    bool newFrame = false;
};
} // namespace hv
