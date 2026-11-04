// LPC analysis (autocorrelation + Levinson-Durbin) and lattice filters.
//
// The tract is represented by reflection coefficients k[1..p]. Interpolating k between frames keeps
// every intermediate filter stable (|k| < 1), and the all-zero lattice (analysis) and all-pole lattice
// (synthesis) are exact inverses even when k changes every chunk — so "inverse filter → process
// excitation → re-apply tract" reconstructs perfectly when the processing is identity.
#pragma once
#include "Common.h"

namespace hv
{
constexpr int kMaxLpcOrder = 40;
constexpr int kLpcChunk    = 16;   // samples per interpolated coefficient set
constexpr int kMaxFormants = 5;
constexpr int kEnvPoints   = 256;  // LPC envelope display resolution (0 .. kEnvMaxHz)
constexpr float kEnvMaxHz  = 8000.0f;

/** LPC order scaled with sample rate (24 at 44.1k … 40 at >= 88.2k). */
inline int lpcOrderForRate (double sr) noexcept
{
    return std::clamp ((int) std::lround (sr / 2000.0) + 2, 24, kMaxLpcOrder);
}

struct Formants
{
    float freq[kMaxFormants] {};
    float bw[kMaxFormants] {};
    int   count = 0;
};

namespace lpc
{
    /** Levinson-Durbin. r[0..p] autocorrelation. Writes k[0..p-1] (k[i] = k_{i+1}) and returns error. */
    double levinson (const double* r, int p, float* k, double* aOut = nullptr) noexcept;
    /** Reflection → direct-form a[0..p] (a[0] = 1). */
    void stepUp (const float* k, int p, double* a) noexcept;
    /** Evaluate 20log10(1/|A(e^jw)|) on kEnvPoints frequencies 0..kEnvMaxHz. */
    void envelopeDb (const double* a, int p, double sr, float gain, float* outDb) noexcept;
    /** Peak-pick formants from an envelope (dB) produced by envelopeDb. */
    void pickFormants (const float* envDb, Formants& out) noexcept;
} // namespace lpc

/** All-zero lattice: x → residual e (prediction error). */
struct LatticeAnalysis
{
    float b[kMaxLpcOrder + 1] {};
    void reset() noexcept { std::fill (std::begin (b), std::end (b), 0.0f); }
    inline float process (float x, const float* k, int p) noexcept
    {
        float f = x, bPrev = x;   // b_0(n) = x
        for (int m = 0; m < p; ++m)
        {
            const float bm1 = b[m];                 // b_{m}(n-1)  (stage m input, delayed)
            const float fNew = f + k[m] * bm1;
            const float bNew = bm1 + k[m] * f;
            b[m] = bPrev;                           // store b_m(n) for next sample
            bPrev = bNew;
            f = fNew;
        }
        return f;
    }
};

/** All-pole lattice: residual e → x. Exact inverse of LatticeAnalysis for the same k sequence. */
struct LatticeSynthesis
{
    float b[kMaxLpcOrder + 1] {};
    void reset() noexcept { std::fill (std::begin (b), std::end (b), 0.0f); }
    inline float process (float e, const float* k, int p) noexcept
    {
        float f = e;
        for (int m = p - 1; m >= 0; --m)
        {
            f -= k[m] * b[m];                       // f_{m}(n) from f_{m+1}(n)
            b[m + 1] = b[m] + k[m] * f;             // b_{m+1}(n)
        }
        b[0] = f;                                    // b_0(n) = f_0(n) = output
        return sanitize (f, 1.0e4f);
    }
};

struct PreEmphasis
{
    float z = 0.0f, coeff = 0.97f;
    void reset() noexcept { z = 0.0f; }
    inline float process (float x) noexcept { const float y = x - coeff * z; z = x; return y; }
};
struct DeEmphasis
{
    float z = 0.0f, coeff = 0.97f;
    void reset() noexcept { z = 0.0f; }
    inline float process (float x) noexcept { z = x + coeff * z; return z; }
};

/** Streaming LPC analyzer: 25 ms Hann frames, 5 ms hop, pre-emphasis 0.97, k interpolated per
    16-sample chunk (reflection domain). Usable by the shared analysis engine and by modules in
    "Analyze: Local" mode. Per-sample API so arbitrary host block sizes work. */
class LpcAnalyzer
{
public:
    void prepare (double sampleRate, bool computeFormants);
    void reset();

    /** Feed one raw sample; returns the pre-emphasised residual. currentK() is valid afterwards. */
    float processSample (float x) noexcept;

    /** True if the last processSample() started a new 16-sample coefficient chunk. */
    bool chunkStarted() const noexcept { return newChunk; }
    int order() const noexcept { return p; }
    const float* currentK() const noexcept { return kCur; }
    float currentGain() const noexcept { return gCur; }
    const Formants& formants() const noexcept { return fm; }
    const float* envelope() const noexcept { return envDb; }   // kEnvPoints, latest frame
    bool frameJustAnalysed() const noexcept { return newFrame; }

private:
    void analyseFrame() noexcept;

    double sr = 48000.0;
    int p = 24, frameLen = 1200, hop = 240, hopCount = 0, ringPos = 0, chunkPhase = 0;
    bool wantFormants = false, newChunk = false, newFrame = false;
    std::vector<float> ring, window, frameBuf;
    std::vector<double> rLag, lagWin;
    PreEmphasis pre;
    LatticeAnalysis inv;
    float kPrev[kMaxLpcOrder] {}, kTarget[kMaxLpcOrder] {}, kCur[kMaxLpcOrder] {};
    float gPrev = 0.0f, gTarget = 0.0f, gCur = 0.0f;
    int   interpPos = 0;
    Formants fm;
    float envDb[kEnvPoints] {};
};
} // namespace hv
