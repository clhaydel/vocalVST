#include "Lpc.h"

namespace hv
{
namespace lpc
{
double levinson (const double* r, int p, float* k, double* aOut) noexcept
{
    double a[kMaxLpcOrder + 1] {}, tmp[kMaxLpcOrder + 1] {};
    a[0] = 1.0;
    double err = r[0];
    for (int i = 1; i <= p; ++i)
    {
        if (err <= 1.0e-12 * r[0] || err <= 0.0)
        {
            for (int j = i; j <= p; ++j) k[j - 1] = 0.0f;
            break;
        }
        double acc = r[i];
        for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
        double ki = -acc / err;
        ki = std::clamp (ki, -0.9995, 0.9995);  // guarantee a strictly stable lattice
        for (int j = 1; j < i; ++j) tmp[j] = a[j] + ki * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = ki;
        k[i - 1] = (float) ki;
        err *= (1.0 - ki * ki);
    }
    if (aOut != nullptr)
        for (int i = 0; i <= p; ++i) aOut[i] = a[i];
    return std::max (err, 0.0);
}

void stepUp (const float* k, int p, double* a) noexcept
{
    double tmp[kMaxLpcOrder + 1] {};
    a[0] = 1.0;
    for (int i = 1; i <= p; ++i) a[i] = 0.0;
    for (int i = 1; i <= p; ++i)
    {
        const double ki = k[i - 1];
        for (int j = 1; j < i; ++j) tmp[j] = a[j] + ki * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = ki;
    }
}

void envelopeDb (const double* a, int p, double sr, float gain, float* outDb) noexcept
{
    const double nyq = sr * 0.5;
    const double g   = std::max (1.0e-9, (double) gain);
    for (int i = 0; i < kEnvPoints; ++i)
    {
        const double f = std::min ((double) i * kEnvMaxHz / (kEnvPoints - 1), nyq * 0.995);
        const double w = 2.0 * kPiD * f / sr;
        double re = 0.0, im = 0.0;
        for (int j = 0; j <= p; ++j)
        {
            re += a[j] * std::cos (w * j);
            im -= a[j] * std::sin (w * j);
        }
        const double mag = std::sqrt (re * re + im * im);
        outDb[i] = (float) (20.0 * std::log10 (g / std::max (mag, 1.0e-9)));
    }
}

void pickFormants (const float* envDb, Formants& out) noexcept
{
    const float binHz = kEnvMaxHz / (kEnvPoints - 1);
    out.count = 0;
    for (int i = 2; i < kEnvPoints - 1 && out.count < kMaxFormants; ++i)
    {
        const float f = (float) i * binHz;
        if (f < 90.0f) continue;
        if (envDb[i] > envDb[i - 1] && envDb[i] >= envDb[i + 1])
        {
            const float y0 = envDb[i - 1], y1 = envDb[i], y2 = envDb[i + 1];
            const float den = y0 - 2.0f * y1 + y2;
            const float off = std::abs (den) > 1.0e-9f ? clampf (0.5f * (y0 - y2) / den, -0.5f, 0.5f) : 0.0f;
            const float peakDb = y1 - 0.25f * (y0 - y2) * off;
            // -3 dB bandwidth
            int lo = i, hi = i;
            while (lo > 0 && envDb[lo] > peakDb - 3.0f) --lo;
            while (hi < kEnvPoints - 1 && envDb[hi] > peakDb - 3.0f) ++hi;
            float bw = (float) (hi - lo) * binHz;
            if (lo == 0 || hi == kEnvPoints - 1) bw = std::max (bw, 300.0f);
            out.freq[out.count] = ((float) i + off) * binHz;
            out.bw[out.count]   = clampf (bw, 40.0f, 800.0f);
            ++out.count;
        }
    }
}
} // namespace lpc

//==============================================================================
void LpcAnalyzer::prepare (double sampleRate, bool computeFormants)
{
    sr           = sampleRate;
    wantFormants = computeFormants;
    p            = lpcOrderForRate (sr);
    frameLen     = (int) std::lround (sr * 0.025);
    hop          = std::max (kLpcChunk, (int) std::lround (sr * 0.005 / kLpcChunk) * kLpcChunk);
    ring.assign ((size_t) frameLen, 0.0f);
    frameBuf.assign ((size_t) frameLen, 0.0f);
    window.resize ((size_t) frameLen);
    for (int i = 0; i < frameLen; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * ((float) i + 0.5f) / (float) frameLen);
    rLag.assign ((size_t) kMaxLpcOrder + 1, 0.0);
    lagWin.resize ((size_t) kMaxLpcOrder + 1);
    for (int i = 0; i <= kMaxLpcOrder; ++i)
    {
        const double x = 2.0 * kPiD * 50.0 * i / sr;    // 50 Hz Gaussian lag window (bandwidth expansion)
        lagWin[(size_t) i] = std::exp (-0.5 * x * x);
    }
    reset();
}

void LpcAnalyzer::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    ringPos = 0;
    hopCount = 0;
    interpPos = 0;
    chunkPhase = 0;
    newChunk = newFrame = false;
    pre.reset();
    inv.reset();
    std::fill (std::begin (kPrev), std::end (kPrev), 0.0f);
    std::fill (std::begin (kTarget), std::end (kTarget), 0.0f);
    std::fill (std::begin (kCur), std::end (kCur), 0.0f);
    gPrev = gTarget = gCur = 0.0f;
    fm = {};
    std::fill (std::begin (envDb), std::end (envDb), -120.0f);
}

float LpcAnalyzer::processSample (float x) noexcept
{
    newFrame = false;
    newChunk = (chunkPhase == 0);
    if (newChunk)
    {   // interpolate k between the previous and latest frame across one hop
        const float t = clampf ((float) interpPos / (float) hop, 0.0f, 1.0f);
        for (int m = 0; m < p; ++m) kCur[m] = kPrev[m] + t * (kTarget[m] - kPrev[m]);
        gCur = gPrev + t * (gTarget - gPrev);
    }
    if (++chunkPhase >= kLpcChunk) chunkPhase = 0;

    const float pe = pre.process (sanitize (x));
    ring[(size_t) ringPos] = pe;
    if (++ringPos >= frameLen) ringPos = 0;
    const float e = inv.process (pe, kCur, p);

    ++interpPos;
    if (++hopCount >= hop)
    {
        hopCount = 0;
        std::copy (kCur, kCur + p, kPrev);
        gPrev = gCur;
        analyseFrame();
        newFrame = true;
        interpPos = 0;
        chunkPhase = 0;   // a new chunk always starts at a hop boundary
    }
    return e;
}

void LpcAnalyzer::analyseFrame() noexcept
{
    double energy = 0.0, wsum = 0.0;
    for (int i = 0; i < frameLen; ++i)
    {
        const float s = ring[(size_t) ((ringPos + i) % frameLen)] * window[(size_t) i];
        frameBuf[(size_t) i] = s;
        energy += (double) s * s;
        wsum += (double) window[(size_t) i] * window[(size_t) i];
    }
    if (energy < 1.0e-12 * frameLen)
    {
        std::fill (kTarget, kTarget + p, 0.0f);
        gTarget = 0.0f;
        if (wantFormants) { std::fill (std::begin (envDb), std::end (envDb), -120.0f); fm.count = 0; }
        return;
    }
    for (int lag = 0; lag <= p; ++lag)
    {
        double acc = 0.0;
        const float* f = frameBuf.data();
        for (int i = lag; i < frameLen; ++i) acc += (double) f[i] * f[i - lag];
        rLag[(size_t) lag] = acc * lagWin[(size_t) lag];
    }
    rLag[0] *= 1.0001;   // white-noise correction (-40 dB)
    double a[kMaxLpcOrder + 1];
    const double err = lpc::levinson (rLag.data(), p, kTarget, a);
    gTarget = (float) std::sqrt (err / std::max (wsum, 1.0));

    if (wantFormants)
    {
        lpc::envelopeDb (a, p, sr, gTarget, envDb);
        lpc::pickFormants (envDb, fm);
    }
}
} // namespace hv
