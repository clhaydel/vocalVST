#include "TestHelpers.h"
#include "dsp/Fft.h"

namespace hvtest
{
using namespace hv;

Signal silence (int n) { return Signal ((size_t) n, 0.0f); }
Signal whiteNoise (int n, float amp, uint64_t seed)
{
    Pcg32 r (seed);
    Signal s ((size_t) n);
    for (auto& x : s) x = r.bipolar() * amp;
    return s;
}
Signal square (int n, double sr, double freq, float amp)
{
    Signal s ((size_t) n);
    for (int i = 0; i < n; ++i) s[(size_t) i] = std::fmod (i * freq / sr, 1.0) < 0.5 ? amp : -amp;
    return s;
}
Signal dc (int n, float v) { return Signal ((size_t) n, v); }
Signal impulses (int n, int spacing, float amp)
{
    Signal s ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += spacing) s[(size_t) i] = amp;
    return s;
}
Signal sine (int n, double sr, double freq, float amp)
{
    Signal s ((size_t) n);
    for (int i = 0; i < n; ++i) s[(size_t) i] = amp * (float) std::sin (2.0 * kPiD * freq * i / sr);
    return s;
}

namespace
{
/** Rosenberg glottal flow derivative at phase ph (0..1). Sharp negative peak at closure. */
inline float glottalDeriv (double ph, double open = 0.6, double close = 0.16)
{
    if (ph < open)        return (float) (0.5 * kPiD / open * std::sin (kPiD * ph / open));
    if (ph < open + close) return (float) (-kPiD / (2.0 * close) * std::sin (0.5 * kPiD * (ph - open) / close)) * 1.0f;
    return 0.0f;
}

struct FormantBank
{
    Biquad r[5];
    void set (double sr, const float* f, const float* bw)
    {
        for (int i = 0; i < 5; ++i) r[i].resonator (sr, f[i], bw[i]);
    }
    float process (float x) { for (auto& b : r) x = b.process (x); return x; }
};

const float kVowels[][5] = {
    { 730, 1090, 2440, 3400, 4500 },  // a
    { 530, 1840, 2480, 3400, 4500 },  // e
    { 270, 2290, 3010, 3500, 4500 },  // i
    { 570, 840, 2410, 3400, 4500 },   // o
    { 300, 870, 2240, 3400, 4500 },   // u
    { 640, 1190, 2390, 3400, 4500 },  // uh
};
const float kBw[5] = { 70, 90, 120, 150, 200 };
} // namespace

Signal glottalVowel (int n, double sr, double f0, const float* formants, float jitter, uint64_t seed)
{
    Pcg32 r (seed);
    FormantBank fb;
    fb.set (sr, formants != nullptr ? formants : kVowels[0], kBw);
    Signal s ((size_t) n);
    double ph = 0.0, inc = f0 / sr;
    for (int i = 0; i < n; ++i)
    {
        ph += inc;
        if (ph >= 1.0) { ph -= 1.0; inc = f0 * (1.0 + jitter * r.bipolar()) / sr; }
        s[(size_t) i] = fb.process (glottalDeriv (ph) * 0.02f);
    }
    const float pk = peakAbs (s);
    if (pk > 0) for (auto& x : s) x *= 0.5f / pk;
    return s;
}

Signal synthVoice (VoiceStyle style, double sr, double seconds, uint64_t seed)
{
    const int n = (int) (seconds * sr);
    Signal out ((size_t) n, 0.0f);
    Pcg32 rng (seed);
    FormantBank fb;
    Biquad fricBp, fricBp2;
    OnePole aspLp;
    aspLp.setCutoff (sr, 6000.0);

    // A "script" of syllables: consonant type, vowel, durations, f0 contour
    struct Syl { int cons; int vowel; double dur; double f0a, f0b; };
    std::vector<Syl> script;
    double total = 0.0;
    while (total < seconds)
    {
        Syl sy;
        sy.cons = rng.below (5);          // 0 none, 1 plosive, 2 's', 3 'sh', 4 nasal-ish (voiced onset)
        sy.vowel = rng.below (6);
        switch (style)
        {
            case VoiceStyle::Spoken:    sy.dur = rng.range (0.16, 0.32); sy.f0a = rng.range (100, 160); sy.f0b = sy.f0a * rng.range (0.8, 1.1); break;
            case VoiceStyle::Sung:      sy.dur = rng.range (0.4, 0.9);  sy.f0a = 110.0 * std::pow (2.0, rng.below (12) / 12.0) * 1.5; sy.f0b = sy.f0a; break;
            case VoiceStyle::Screamed:  sy.dur = rng.range (0.25, 0.6); sy.f0a = rng.range (280, 520); sy.f0b = sy.f0a * rng.range (0.85, 1.2); break;
            case VoiceStyle::Whispered: sy.dur = rng.range (0.18, 0.34); sy.f0a = sy.f0b = 0; break;
        }
        script.push_back (sy);
        total += sy.dur + (rng.uniform() < 0.25f ? 0.12 : 0.02);
    }

    double t = 0.0, ph = 0.0;
    float curF[5]; std::copy (kVowels[0], kVowels[0] + 5, curF);
    float shimmer = 1.0f;
    for (const auto& sy : script)
    {
        const int start = (int) (t * sr);
        const int consLen = sy.cons == 0 || sy.cons == 4 ? 0 : (int) ((sy.cons == 1 ? 0.06 : 0.11) * sr);
        const int len = (int) (sy.dur * sr);
        // consonant
        for (int i = 0; i < consLen && start + i < n; ++i)
        {
            float x = 0.0f;
            const double tt = (double) i / sr;
            if (sy.cons == 1)
            {   // plosive: 40 ms closure then a 15 ms broadband burst decaying into aspiration
                if (tt > 0.04) x = rng.bipolar() * 0.5f * (float) std::exp (-(tt - 0.04) / 0.008);
            }
            else
            {
                if (i == 0) { fricBp.bandpass (sr, sy.cons == 2 ? 6500.0 : 3200.0, 2.0); fricBp2.bandpass (sr, sy.cons == 2 ? 7500.0 : 2800.0, 1.5); }
                const float env = (float) std::sin (kPiD * i / consLen);
                x = (fricBp.process (rng.bipolar()) + 0.5f * fricBp2.process (rng.bipolar())) * 0.35f * env;
            }
            out[(size_t) (start + i)] += x;
        }
        // vowel nucleus with formant glide from the previous vowel
        const int vStart = start + consLen;
        const float* target = kVowels[sy.vowel];
        for (int i = 0; i < len - consLen && vStart + i < n; ++i)
        {
            const double frac = (double) i / std::max (1, len - consLen);
            const float glide = (float) std::min (1.0, frac * 6.0);
            float f[5];
            for (int k = 0; k < 5; ++k) f[k] = curF[k] + (target[k] - curF[k]) * glide;
            if ((i & 31) == 0)
            {
                float bw[5];
                const float bwMul = style == VoiceStyle::Screamed ? 1.6f : 1.0f;
                for (int k = 0; k < 5; ++k) bw[k] = kBw[k] * bwMul;
                if (style == VoiceStyle::Screamed) { f[0] *= 1.25f; f[1] *= 1.1f; }   // open-mouth scream raises F1
                fb.set (sr, f, bw);
            }
            const float amp = (float) (std::sin (kPiD * std::min (1.0, frac * 1.0)) * 0.7 + 0.3) * (float) std::min (1.0, (1.0 - frac) * 8.0);
            float x;
            if (style == VoiceStyle::Whispered)
                x = aspLp.lp (rng.bipolar()) * 0.06f;
            else
            {
                double f0 = sy.f0a + (sy.f0b - sy.f0a) * frac;
                if (style == VoiceStyle::Sung) f0 *= 1.0 + 0.012 * std::sin (2 * kPiD * 5.5 * i / sr) * std::min (1.0, frac * 3);
                const double jit = style == VoiceStyle::Screamed ? 0.04 : 0.008;
                ph += f0 / sr;
                if (ph >= 1.0)
                {
                    ph -= 1.0;
                    ph += jit * rng.bipolar() * 0.5;
                    if (ph < 0) ph = 0;
                    shimmer = 1.0f + (style == VoiceStyle::Screamed ? 0.25f : 0.05f) * rng.bipolar();
                }
                x = glottalDeriv (ph, 0.55, style == VoiceStyle::Screamed ? 0.08 : 0.16) * 0.01f * shimmer;
                const float asp = style == VoiceStyle::Screamed ? 0.012f : 0.002f;
                x += aspLp.lp (rng.bipolar()) * asp * (ph > 0.5 ? 1.0f : 0.3f);
                if (style == VoiceStyle::Screamed && rng.uniform() < 0.00015f) ph = 0.3;   // occasional pitch breaks
            }
            out[(size_t) (vStart + i)] += fb.process (x) * amp;
        }
        std::copy (target, target + 5, curF);
        t += sy.dur + (rng.uniform() < 0.25f ? 0.12 : 0.02);
    }
    // normalise to -3 dBFS peak, with a gentle soft clip for the scream
    float pk = peakAbs (out);
    const float g = pk > 0 ? 0.7f / pk : 1.0f;
    for (auto& x : out)
    {
        x *= g;
        if (style == VoiceStyle::Screamed) x = std::tanh (2.0f * x) * 0.6f;
    }
    return out;
}

bool writeWav (const juce::File& f, const std::vector<Signal>& channels, double sr)
{
    f.deleteFile();
    f.getParentDirectory().createDirectory();
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) channels.size(), 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    const int n = (int) channels[0].size();
    std::vector<const float*> ptrs;
    for (auto& c : channels) ptrs.push_back (c.data());
    return w->writeFromFloatArrays (ptrs.data(), (int) ptrs.size(), n);
}

bool readWav (const juce::File& f, std::vector<Signal>& channels, double& sr)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return false;
    sr = r->sampleRate;
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    channels.assign ((size_t) b.getNumChannels(), {});
    for (int c = 0; c < b.getNumChannels(); ++c)
        channels[(size_t) c].assign (b.getReadPointer (c), b.getReadPointer (c) + b.getNumSamples());
    return true;
}

//==============================================================================
ParamStore::ParamStore()
{
    const auto defs = params::buildParamDefs();
    for (const auto& d : defs) v.push_back (std::make_unique<std::atomic<float>> (d.def));
}

params::ParamRefs ParamStore::refs() const
{
    params::ParamRefs r;
    for (size_t i = 0; i < v.size(); ++i) r.p[i] = v[i].get();
    return r;
}

void ParamStore::setSlot (int s, ModuleType t)
{
    using namespace params;
    set (slotIndex (s, SType), (float) (int) t);
    set (slotIndex (s, SOn), 1.0f);
    set (slotIndex (s, SMix), 100.0f);
    set (slotIndex (s, SOut), 0.0f);
    const auto& specs = moduleInfo (t).params;
    for (int k = 0; k < kParamsPerSlot; ++k)
        set (slotIndex (s, SP0 + k), k < (int) specs.size() ? specs[(size_t) k].toNorm (specs[(size_t) k].def) : 0.0f);
}

void ParamStore::setSlotParam (int s, const char* name, float real)
{
    using namespace params;
    const auto t = (ModuleType) (int) get (slotIndex (s, SType));
    const int k = paramIndex (t, name);
    if (k >= 0) set (slotIndex (s, SP0 + k), moduleInfo (t).params[(size_t) k].toNorm (real));
}

//==============================================================================
ModuleRig::ModuleRig (ModuleType t, double sampleRate, uint64_t sd) : type (t), sr (sampleRate), seed (sd)
{
    mod = createModule (t);
    mod->prepare (sr, kMaxSubBlock);
    ana.prepare (sr, kMaxSubBlock);
    const auto& specs = moduleInfo (t).params;
    for (size_t k = 0; k < specs.size(); ++k) values[k] = specs[k].def;
}

void ModuleRig::setParam (const char* name, float real)
{
    const int k = paramIndex (type, name);
    if (k >= 0) values[k] = real;
}

std::vector<Signal> ModuleRig::run (const Signal& in, int block)
{
    const int n = (int) in.size();
    std::vector<Signal> out (2, Signal ((size_t) n, 0.0f));
    juce::AudioBuffer<float> store (2, kMaxSubBlock), view;
    AnalysisFrame frame;
    // module "Mix"/latency are applied by the engine, not here: raw module output
    bool stereo = false;
    for (int pos = 0; pos < n;)
    {
        const int len = std::min ({ block, n - pos, kMaxSubBlock });
        ana.process (in.data() + pos, len);
        ana.makeFrame (0, len, frame);
        view.setDataToReferTo (store.getArrayOfWritePointers(), 2, len);
        for (int c = 0; c < 2; ++c) std::copy (in.begin() + pos, in.begin() + pos + len, view.getWritePointer (c));
        ProcessContext ctx;
        ctx.values = values;
        ctx.liveNorm = live;
        ctx.numChannels = stereo ? 2 : 1;
        ctx.seed = seed;
        ctx.shared = &shared;
        mod->setContext (ctx);
        mod->process (view, frame);
        stereo = stereo || mod->producesStereo();
        for (int c = 0; c < 2; ++c)
            std::copy (view.getReadPointer (stereo ? c : 0), view.getReadPointer (stereo ? c : 0) + len, out[(size_t) c].begin() + pos);
        pos += len;
    }
    return out;
}

float peakAbs (const Signal& s, int from)
{
    float p = 0.0f;
    for (size_t i = (size_t) std::max (0, from); i < s.size(); ++i) p = std::max (p, std::abs (s[i]));
    return p;
}
float rms (const Signal& s, int from, int to)
{
    if (to < 0) to = (int) s.size();
    double a = 0.0;
    for (int i = from; i < to; ++i) a += (double) s[(size_t) i] * s[(size_t) i];
    return to > from ? (float) std::sqrt (a / (to - from)) : 0.0f;
}
bool allFinite (const Signal& s)
{
    for (float x : s) if (! std::isfinite (x)) return false;
    return true;
}
double spectralPeak (const Signal& s, double sr, double lo, double hi)
{
    // Welch-averaged magnitude spectrum (Hann, 50% overlap) so noise-driven responses are stable
    int n = 1;
    while (n < (int) s.size() && n < 16384) n <<= 1;
    if (n > (int) s.size()) n >>= 1;
    n = std::max (n, 256);
    Fft fft; fft.prepare (n);
    std::vector<float> buf ((size_t) n), mag ((size_t) n / 2 + 1), acc ((size_t) n / 2 + 1, 0.0f);
    int frames = 0;
    for (size_t pos = 0; pos + (size_t) n <= s.size() || frames == 0; pos += (size_t) n / 2)
    {
        for (int i = 0; i < n; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) n);
            buf[(size_t) i] = pos + (size_t) i < s.size() ? s[pos + (size_t) i] * w : 0.0f;
        }
        fft.magnitude (buf.data(), mag.data());
        for (size_t b = 0; b < mag.size(); ++b) acc[b] += mag[b] * mag[b];
        ++frames;
        if (pos + (size_t) n >= s.size()) break;
    }
    const double bin = sr / n;
    int best = std::max (1, (int) (lo / bin));
    for (int b = best; b <= (int) (hi / bin) && b < (int) acc.size() - 1; ++b)
        if (acc[(size_t) b] > acc[(size_t) best]) best = b;
    if (best > 0 && best < (int) acc.size() - 1)
    {
        const double a = std::log (acc[(size_t) best - 1] + 1e-20), b = std::log (acc[(size_t) best] + 1e-20), c = std::log (acc[(size_t) best + 1] + 1e-20);
        const double d = a - 2 * b + c;
        const double off = std::abs (d) > 1e-12 ? 0.5 * (a - c) / d : 0.0;
        return (best + std::clamp (off, -0.5, 0.5)) * bin;
    }
    return best * bin;
}
} // namespace hvtest
