// Harsh Vox — shared DSP utilities (JUCE-free, header-only).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <vector>

namespace hv
{
constexpr float kPi    = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr double kPiD  = std::numbers::pi;

inline float dbToGain (float db) noexcept { return db <= -120.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return g <= 1.0e-6f ? -120.0f : 20.0f * std::log10 (g); }
inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float smoothstep (float e0, float e1, float x) noexcept
{
    const float t = clampf ((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline bool isFiniteF (float x) noexcept { return std::isfinite (x); }
/** Replace NaN/Inf with 0 and clamp to a sane range; used at module boundaries. */
inline float sanitize (float x, float lim = 64.0f) noexcept { return std::isfinite (x) ? clampf (x, -lim, lim) : 0.0f; }

/** Cheap tanh approximation (Padé 7/6 with clamp) — accurate to ~1e-4, monotonic. */
inline float fastTanh (float x) noexcept
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float a  = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b  = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

/** Time constant -> one-pole coefficient (for y += c * (x - y)). */
inline float onePoleCoeff (float ms, double sr) noexcept
{
    if (ms <= 0.0f) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (0.001 * ms * sr)));
}

//==============================================================================
/** PCG32 — small, fast, deterministic PRNG. All randomness in the plugin goes through this. */
class Pcg32
{
public:
    Pcg32 (uint64_t seed = 0x853c49e6748fea9bULL, uint64_t stream = 0xda3e39cb94b95bdbULL) noexcept { seedWith (seed, stream); }

    void seedWith (uint64_t seed, uint64_t stream = 0xda3e39cb94b95bdbULL) noexcept
    {
        state = 0u;
        inc   = (stream << 1u) | 1u;
        next();
        state += seed;
        next();
    }

    uint32_t next() noexcept
    {
        const uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        const auto xorshifted = (uint32_t) (((old >> 18u) ^ old) >> 27u);
        const auto rot        = (uint32_t) (old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }           // [0,1)
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }                                   // [-1,1)
    float range (float lo, float hi) noexcept { return lo + (hi - lo) * uniform(); }
    int   below (int n) noexcept { return n <= 1 ? 0 : (int) (next() % (uint32_t) n); }
    /** Exponential variate with given mean (for Poisson inter-arrival times). */
    float exponential (float mean) noexcept { return -mean * std::log (1.0f - uniform() * 0.999999f); }

private:
    uint64_t state = 0, inc = 0;
};

/** Hash helper to derive independent seeds (slot, module, purpose) from the global seed. */
inline uint64_t mixSeed (uint64_t a, uint64_t b) noexcept
{
    uint64_t z = a + 0x9e3779b97f4a7c15ULL + (b << 6) + (b >> 2);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

//==============================================================================
/** Linear ramp smoother. Every continuous parameter goes through one of these (default 20 ms). */
class Smoother
{
public:
    void prepare (double sr, float ms = 20.0f) noexcept
    {
        rampLen = std::max (1, (int) std::lround (sr * 0.001 * ms));
    }
    void snap (float v) noexcept { cur = target = v; steps = 0; inc = 0.0f; }
    void setTarget (float v) noexcept
    {
        if (v == target) return;
        target = v;
        steps  = rampLen;
        inc    = (target - cur) / (float) steps;
    }
    float next() noexcept
    {
        if (steps > 0) { cur += inc; if (--steps == 0) cur = target; }
        return cur;
    }
    /** Advance n samples, return value at end (for block-rate coefficient updates). */
    float skip (int n) noexcept
    {
        if (steps <= 0) return cur;
        if (n >= steps) { cur = target; steps = 0; return cur; }
        cur += inc * (float) n; steps -= n;
        return cur;
    }
    float current() const noexcept { return cur; }
    float getTarget() const noexcept { return target; }
    bool  isSmoothing() const noexcept { return steps > 0; }

private:
    float cur = 0.0f, target = 0.0f, inc = 0.0f;
    int   steps = 0, rampLen = 960;
};

//==============================================================================
/** Biquad, transposed direct form II. Coefficients designed in double (RBJ cookbook). */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        const double ia = 1.0 / A0;
        b0 = (float) (B0 * ia); b1 = (float) (B1 * ia); b2 = (float) (B2 * ia);
        a1 = (float) (A1 * ia); a2 = (float) (A2 * ia);
    }

    static double clampFreq (double f, double sr) { return std::clamp (f, 5.0, sr * 0.49); }

    void lowpass (double sr, double f, double q = 0.70710678)
    {
        const double w = 2.0 * kPiD * clampFreq (f, sr) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void highpass (double sr, double f, double q = 0.70710678)
    {
        const double w = 2.0 * kPiD * clampFreq (f, sr) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    /** Band-pass, 0 dB peak gain. */
    void bandpass (double sr, double f, double q)
    {
        const double w = 2.0 * kPiD * clampFreq (f, sr) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set (a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    void peak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * clampFreq (f, sr) / sr;
        const double c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void lowShelf (double sr, double f, double gainDb, double s = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * clampFreq (f, sr) / sr;
        const double c = std::cos (w), sn = std::sin (w);
        const double a = sn / 2.0 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), sa = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double sr, double f, double gainDb, double s = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * clampFreq (f, sr) / sr;
        const double c = std::cos (w), sn = std::sin (w);
        const double a = sn / 2.0 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), sa = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
    /** Klatt-style formant resonator with unity DC gain: frequency + bandwidth in Hz. */
    void resonator (double sr, double f, double bw)
    {
        f  = clampFreq (f, sr);
        bw = std::clamp (bw, 10.0, sr * 0.25);
        const double r = std::exp (-kPiD * bw / sr), c = 2.0 * r * std::cos (2.0 * kPiD * f / sr);
        const double A1 = -c, A2 = r * r;
        const double g = 1.0 + A1 + A2;          // unity gain at DC
        set (g, 0, 0, 1, A1, A2);
    }

    /** Magnitude response at frequency f (for tests/UI). */
    double magnitude (double sr, double f) const
    {
        const double w = 2.0 * kPiD * f / sr;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
        const double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

/** One-pole low-pass / high-pass. */
struct OnePole
{
    float c = 1.0f, z = 0.0f;
    void setCutoff (double sr, double f) noexcept
    {
        c = (float) (1.0 - std::exp (-2.0 * kPiD * std::clamp (f, 1.0, sr * 0.49) / sr));
    }
    void reset() noexcept { z = 0.0f; }
    inline float lp (float x) noexcept { z += c * (x - z); return z; }
    inline float hp (float x) noexcept { z += c * (x - z); return x - z; }
};

/** DC blocker (one-zero one-pole), cutoff derived from sample rate. */
struct DcBlocker
{
    float R = 0.995f, x1 = 0.0f, y1 = 0.0f;
    void prepare (double sr, double fc = 10.0) noexcept { R = (float) std::exp (-2.0 * kPiD * fc / sr); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    inline float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return y;
    }
};

//==============================================================================
/** Power-of-two ring buffer delay line with fractional (linear / cubic) reads. Preallocated. */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    /** Integer delay; d = 0 returns the most recently pushed sample. */
    inline float tap (int d) const noexcept { return buf[(size_t) ((w - 1 - d) & mask)]; }
    inline float tapLinear (float d) const noexcept
    {
        const int   i = (int) d;
        const float f = d - (float) i;
        return tap (i) + f * (tap (i + 1) - tap (i));
    }
    inline float tapCubic (float d) const noexcept
    {
        const int   i = (int) d;
        const float f = d - (float) i;
        const float xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    int size() const noexcept { return mask + 1; }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

/** Simple integer-delay compensation line for whole buffers (used for dry paths). */
class CompDelay
{
public:
    void prepare (int maxDelay) { line.prepare (std::max (1, maxDelay)); }
    void reset() noexcept { line.reset(); }
    inline float process (float x, int delay) noexcept
    {
        line.push (x);
        return line.tap (std::clamp (delay, 0, maxDelay()));
    }
    int maxDelay() const noexcept { return line.size() - 4; }

private:
    DelayLine line;
};

//==============================================================================
/** Peak/RMS hybrid envelope follower with attack/release. */
class EnvelopeFollower
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        rmsCoeff   = onePoleCoeff (5.0f, sr);
        setTimes (2.0f, 60.0f);
        reset();
    }
    void setTimes (float attackMs, float releaseMs) noexcept
    {
        atk = onePoleCoeff (attackMs, sampleRate);
        rel = onePoleCoeff (releaseMs, sampleRate);
    }
    void reset() noexcept { ms = 0.0f; env = 0.0f; }
    inline float process (float x) noexcept
    {
        const float a = std::abs (x);
        ms += rmsCoeff * (x * x - ms);
        const float in = 0.5f * (a + std::sqrt (std::max (0.0f, ms)) * 1.41421356f);
        env += (in > env ? atk : rel) * (in - env);
        return env;
    }
    float value() const noexcept { return env; }

private:
    double sampleRate = 48000.0;
    float  atk = 0.1f, rel = 0.001f, rmsCoeff = 0.01f, ms = 0.0f, env = 0.0f;
};

/** Map an envelope in dB above a threshold onto 0..1 over a 24 dB span (used by every "Reacts" mapping). */
inline float reactDrive (float envDb, float thresholdDb, float spanDb = 24.0f) noexcept
{
    return clampf ((envDb - thresholdDb) / spanDb, 0.0f, 1.0f);
}

} // namespace hv
