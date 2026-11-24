#pragma once

// WowLab engine: wow & flutter pitch modulation with tape colour (original DSP, independent of JUCE).
//
// Chain (wet): input gain -> tape colour (EQ) -> tape saturation -> modulated delay (wow, flutter, random,
//              stereo phase, wet delay; Hermite or high-quality windowed-sinc interpolation) -> noise
// Colour and saturation curves of the "15 IPS" model are fitted on measurements (log sweeps, 1 kHz level
// ramps) of a reference plugin; "7.5 IPS" and "Cassette" are provisional until measured.
// Mix: "Morph" scales every amount with a single signal path (no comb filtering);
//      "Parallel" blends with the latency-aligned dry signal (chorus / flanging on purpose).

#include "../RetroDSP.h"
#include <array>

namespace wow
{
using retro::Biquad;
using retro::Rng;
using retro::AdaaSat;
using retro::DcBlocker;
using retro::kPi;
using retro::dbToGain;

// Colour = low shelf + high shelf whose gains follow a curve over the Color knob (0, .2, .4, .6, .8, 1, 1.25, 1.5, 1.75, 2)
struct AnalogModel
{
    const char* name; double snrDb, noiseLpHz, lsHz, lsQ, hsHz, hsQ, pkHz;
    std::array<double, 10> lsDb, hsDb, pkDb;
    bool measured;
};
static const AnalogModel kModels[] =
{
    { "15 IPS",   -68.0, 20000.0, 136.9, 0.51, 12084.0, 0.44, 1051.0,
      { 0.0, 0.39, 0.68, 0.97, 1.26, 1.56, 1.99, 2.42, 2.86, 3.29 },
      { 0.0, -1.29, -2.27, -3.24, -4.22, -5.19, -7.51, -9.83, -12.15, -14.47 },
      { 0.0, 0.0, 0.02, 0.04, 0.06, 0.07, -0.09, -0.25, -0.40, -0.56 }, true },
    { "7.5 IPS",  -67.0, 13000.0, 130.0, 0.55, 9500.0, 0.45, 1000.0,
      { 0.0, 0.50, 0.88, 1.26, 1.64, 2.00, 2.55, 3.10, 3.65, 4.20 },
      { 0.0, -1.9, -3.4, -4.9, -6.4, -7.9, -11.4, -14.9, -18.4, -21.9 },
      { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.1, -0.3, -0.5, -0.7 }, false },
    { "Cassette", -61.0, 10000.0, 110.0, 0.55, 7000.0, 0.50, 1000.0,
      { 0.0, 0.60, 1.05, 1.50, 1.95, 2.40, 3.00, 3.60, 4.20, 4.80 },
      { 0.0, -2.6, -4.6, -6.6, -8.6, -10.6, -14.5, -18.4, -22.3, -26.2 },
      { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.1, -0.3, -0.5, -0.7 }, false },
};

static double curveAt (const std::array<double, 10>& t, double c)
{
    static const double k[10] = { 0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.25, 1.5, 1.75, 2.0 };
    c = std::clamp (c, 0.0, 2.0);
    for (int i = 1; i < 10; ++i)
        if (c <= k[i]) return t[(size_t) i - 1] + (t[(size_t) i] - t[(size_t) i - 1]) * (c - k[i - 1]) / (k[i] - k[i - 1]);
    return t[9];
}

// RBJ shelf with explicit Q
static void setShelf (Biquad& b, bool high, double fs, double f, double db, double Q)
{
    const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * std::clamp (f, 5.0, fs * 0.45) / fs, c = std::cos (w);
    const double sq = 2 * std::sqrt (A) * (std::sin (w) / (2 * Q));
    if (! high)
        b.set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
               (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    else
        b.set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
               (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
}

// Tape saturation: cubic soft clip (u - u^3/3 up to |u| = 1) followed by a weak linear tail, with
// first-order anti-derivative anti-aliasing. y = g0 * f(a x) / a, so the small-signal gain is g0.
struct TapeSat
{
    static constexpr double kTail = 0.11;
    double x1 = 0.0; bool first = true;
    void reset() { x1 = 0.0; first = true; }
    static double f (double u)
    {
        const double au = std::abs (u), s = u < 0 ? -1.0 : 1.0;
        return au < 1.0 ? u - u * u * u / 3.0 : s * (2.0 / 3.0 + kTail * (au - 1.0));
    }
    static double F (double u)   // antiderivative of f (even)
    {
        const double au = std::abs (u);
        if (au < 1.0) return au * au / 2.0 - au * au * au * au / 12.0;
        const double d = au - 1.0;
        return 5.0 / 12.0 + 2.0 / 3.0 * d + kTail * d * d / 2.0;
    }
    double process (double x, double a, double g0)
    {
        const double u = a * x, u1 = a * x1;
        double y;
        if (first) { y = f (u); first = false; }
        else if (std::abs (u - u1) > 1e-6) y = (F (u) - F (u1)) / (u - u1);
        else y = f (0.5 * (u + u1));
        const double out = g0 / a * (u + (y - 0.5 * (u + u1)));   // only the non-linear residual is averaged: flat linear response
        x1 = x;
        return out;
    }
};

enum RandomTarget { RndOff = 0, RndSpeed, RndAmount, RndShape, RndStereo, RndOffset, RndAmountSpeed, RndWetDelay };

struct Params
{
    double speedHz = 0.55;          // wow speed (already resolved from BPM / note when synced)
    double cycleBeats = 1.0;        // note length in beats when tempo-synced
    bool   synced = false;          // rate follows the host position
    double amount = 0.15;           // 0..1
    double amountMult = 1.0;        // 1..256
    double shape = 0.0;             // -1 square .. 0 sine .. +1 triangle
    double flutterBal = 0.5;        // 0 wow only .. 1 flutter only
    double stereoPhaseDeg = 0.0, phaseOffsetDeg = 0.0, wetDelayMs = 0.0;
    double inGainDb = 0.0, outGainDb = 0.0;

    bool   analogOn = true;
    int    model = 0;
    double color = 1.0, saturation = 1.0, noiseGainDb = -6.0;   // color / saturation 0..2

    int    rndMode = RndOff;
    double rndSpeed = 1.0, rndAmount = 1.0, rndSmooth = 1.0;    // amount 0..2, smoothing 0.25..2
    int    rndSeed = 0;

    double mix = 1.0;
    int    mixMode = 0;             // 0 morph, 1 parallel
    bool   hq = false;
    bool   bypass = false;
};

// 32-tap Kaiser-windowed sinc fractional interpolator (table of 512 fractional phases)
struct SincTable
{
    static constexpr int kTaps = 32, kPhases = 512;
    std::vector<float> c;
    SincTable()
    {
        c.resize ((size_t) (kTaps * (kPhases + 1)));
        auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const double beta = 8.0, cutoff = 0.92, i0b = bessel0 (beta);
        for (int ph = 0; ph <= kPhases; ++ph)
        {
            const double frac = (double) ph / kPhases;
            double sum = 0;
            std::array<double, kTaps> h {};
            for (int j = 0; j < kTaps; ++j)
            {
                const double t = (j - 15) - frac;                     // distance from the read position
                const double x = kPi * cutoff * t;
                const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (x) / x;
                const double r = t / 16.5;
                const double w = std::abs (r) >= 1.0 ? 0.0 : bessel0 (beta * std::sqrt (1.0 - r * r)) / i0b;
                h[(size_t) j] = sinc * w;
                sum += h[(size_t) j];
            }
            for (int j = 0; j < kTaps; ++j) c[(size_t) (ph * kTaps + j)] = (float) (h[(size_t) j] / sum);
        }
    }
};

class Engine
{
public:
    static constexpr double kBaseNormal = 2.0, kBaseHq = 17.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& b : buf) b.init ((int) (fs * 1.2));
        for (auto& b : dryBuf) b.init (64);
        reset();
        setParams (p);
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            buf[c].clear(); dryBuf[c].clear(); sat[c].reset();
            ls[c].reset(); hs[c].reset(); pk[c].reset(); noiseLp[c].reset();
            noiseRng[c].s = 0x9E3779B9u + (uint32_t) c * 2654435761u;
        }
        phase = 0; rndCur = rndTarget = 0; rndStepIndex = -1; freeTime = 0; smoothInit = false; fadeIn = 1.0;
        prevDelay[0] = prevDelay[1] = -1;
    }

    void setParams (const Params& np)
    {
        if (np.model != p.model || np.analogOn != p.analogOn || np.hq != p.hq)
        {
            // structural change: clear the colour filters and fade the wet signal back in (no click)
            for (int c = 0; c < 2; ++c) { ls[c].reset(); hs[c].reset(); pk[c].reset(); sat[c].reset(); }
            fadeIn = 0.0;
        }
        p = np;
        // Morph: a single path, the mix scales every amount
        if (p.mixMode == 0)
        {
            const double m = std::clamp (p.mix, 0.0, 1.0);
            p.amount *= m; p.color *= m; p.saturation *= m; p.wetDelayMs *= m;
            p.noiseGainDb += 20.0 * std::log10 (std::max (m, 1e-5));
        }
        const auto& md = kModels[std::clamp (p.model, 0, 2)];
        const double cc = std::clamp (p.color, 0.0, 2.0);
        for (int c = 0; c < 2; ++c)
        {
            setShelf (ls[c], false, fs, md.lsHz, curveAt (md.lsDb, cc), md.lsQ);
            setShelf (hs[c], true,  fs, md.hsHz, curveAt (md.hsDb, cc), md.hsQ);
            pk[c].peak (fs, md.pkHz, 0.7, curveAt (md.pkDb, cc));
            noiseLp[c].lowpass (fs, md.noiseLpHz, 0.7071);
        }
        noiseGain = dbToGain (md.snrDb + p.noiseGainDb + (p.color > 1.0 ? 6.0 * (p.color - 1.0) : 0.0));
        if (p.noiseGainDb <= -95.9 || ! p.analogOn) noiseGain = 0.0;
        // saturation: cubic drive a (about sqrt of the knob up to 100 %, then steeper) and small-signal gain above 100 %
        const double sv = std::clamp (p.saturation, 0.0, 2.0);
        if (sv <= 1.0) { satA = 0.513 * std::sqrt (sv); satG = 1.0; }
        else
        {
            const double lnA = sv <= 1.25 ? std::log (0.513) + (std::log (0.776) - std::log (0.513)) * (sv - 1.0) / 0.25
                                          : std::log (0.776) + (std::log (2.666) - std::log (0.776)) * (sv - 1.25) / 0.75;
            const double gdb = sv <= 1.25 ? 0.6 * (sv - 1.0) / 0.25 : 0.6 + 2.4 * (sv - 1.25) / 0.75;
            satA = std::exp (lnA); satG = dbToGain (gdb);
        }
        base = p.hq ? kBaseHq : kBaseNormal;
        smoothK = 1.0 - std::exp (-1.0 / (0.06 * fs));
    }

    int latencySamples() const { return (int) base; }

    // Host transport (call once per block). ppq / samplePos are used for sync and repeatable random.
    void setTransport (bool playing, double ppq, double bpm, long long samplePos)
    {
        hostPlaying = playing; hostPpq = ppq; hostBpm = bpm; hostSample = samplePos;
        if (p.synced && playing)
        {
            const double cyc = std::max (p.cycleBeats, 1.0 / 64.0);
            phase = ppq / cyc - std::floor (ppq / cyc);
        }
        blockSample = 0;
    }

    void process (double& l, double& r)
    {
        const double gIn = dbToGain (p.inGainDb), gOut = dbToGain (p.outGainDb);
        double x[2] = { l * gIn, r * gIn };
        const double dry[2] = { l, r };
        dryBuf[0].write ((float) dry[0]); dryBuf[1].write ((float) dry[1]);
        if (p.bypass) { l = dryBuf[0].read (base); r = dryBuf[1].read (base); ++blockSample; return; }

        // ---- random section (repeatable when a seed is set: values depend only on time)
        const double rnd = randomValue();
        double speed = p.speedHz, amount = p.amount, shape = p.shape, stereoDeg = p.stereoPhaseDeg,
               offsetDeg = p.phaseOffsetDeg, wetMs = p.wetDelayMs;
        const double dev = 0.5 * p.rndAmount * rnd;                 // 100 % -> +-50 %
        switch (p.rndMode)
        {
            case RndSpeed:       speed *= 1.0 + dev; break;
            case RndAmount:      amount *= 1.0 + dev; break;
            case RndShape:       shape = std::clamp (shape + 2.0 * dev, -1.0, 1.0); break;
            case RndStereo:      stereoDeg += 360.0 * dev; break;
            case RndOffset:      offsetDeg += 360.0 * dev; break;
            case RndAmountSpeed: speed *= 1.0 + dev; amount *= 1.0 + dev; break;
            case RndWetDelay:    wetMs = std::max (0.0, wetMs * (1.0 + dev)); break;
            default: break;
        }
        speed = std::max (0.01, speed);

        // ---- modulation depth (peak delay excursion), limited so the tape never runs backwards
        const double shapeSlope = shape < 0 ? 1.0 + 3.0 * (-shape) : 1.0 - 0.36 * shape;
        const double flutHz = speed * 8.0;
        double wowA  = amount * p.amountMult * 0.00025 * (1.0 - p.flutterBal);     // seconds
        double flutA = amount * p.amountMult * 0.00025 * p.flutterBal / 8.0;
        const double slope = 2 * kPi * (speed * wowA + flutHz * flutA) * shapeSlope;
        if (slope > 0.9) { const double k = 0.9 / slope; wowA *= k; flutA *= k; }

        // ---- tape: colour EQ, then record saturation
        if (p.analogOn)
            for (int c = 0; c < 2; ++c)
            {
                x[c] = pk[c].process (hs[c].process (ls[c].process (x[c])));
                if (satA > 1e-4) x[c] = sat[c].process (x[c], satA, satG);
            }

        // ---- smoothing: depth, shape, phases and wet delay glide (about 60 ms) instead of jumping,
        //      so automation and preset changes never make the playback head skip (clicks)
        const double k = smoothK;
        if (! smoothInit) { smWow = wowA; smFlut = flutA; smShape = shape; smStereo = stereoDeg; smOffset = offsetDeg; smWet = wetMs; smoothInit = true; }
        smWow += (wowA - smWow) * k; smFlut += (flutA - smFlut) * k; smShape += (shape - smShape) * k;
        smStereo += (stereoDeg - smStereo) * k; smOffset += (offsetDeg - smOffset) * k; smWet += (wetMs - smWet) * k * 0.5;
        wowA = smWow; flutA = smFlut; shape = smShape; stereoDeg = smStereo; offsetDeg = smOffset; wetMs = smWet;

        // ---- modulated read
        double y[2];
        for (int c = 0; c < 2; ++c)
        {
            buf[c].write ((float) x[c]);
            const double ph = phase + (offsetDeg + (c == 1 ? stereoDeg : 0.0)) / 360.0;
            const double wv = wave (ph, shape);
            const double fv = wave (ph * 8.0, shape);
            const double d = base + wetMs * 0.001 * fs + fs * (wowA * (1.0 + wv) + flutA * (1.0 + fv));
            y[c] = p.hq ? readSinc (buf[c], d) : buf[c].read (d);
        }

        // ---- tape noise
        if (p.analogOn && noiseGain > 0.0)
            for (int c = 0; c < 2; ++c) y[c] += noiseLp[c].process (noiseRng[c].next()) * noiseGain * 1.7;

        if (fadeIn < 1.0) fadeIn = std::min (1.0, fadeIn + 1.0 / (0.015 * fs));

        // ---- advance the LFO
        phase += speed / fs;
        phase -= std::floor (phase);
        ++blockSample; freeTime += 1.0 / fs;

        for (int c = 0; c < 2; ++c)
        {
            double o = y[c] * gOut * fadeIn * fadeIn;
            if (p.mixMode == 1)
            {
                const double d = dryBuf[c].read (base);
                o = d * (1.0 - p.mix) + o * p.mix;
            }
            (c == 0 ? l : r) = std::isfinite (o) ? o : 0.0;
        }
    }

private:
    static double wave (double ph, double shape)
    {
        const double s = std::sin (2 * kPi * ph);
        if (shape < 0.0)
        {
            const double sq = std::tanh (4.0 * s) / std::tanh (4.0);
            return (1.0 + shape) * s + (-shape) * sq;
        }
        const double tri = (2.0 / kPi) * std::asin (std::clamp (s, -1.0, 1.0));
        return (1.0 - shape) * s + shape * tri;
    }

    static double hashUnit (uint32_t seed, long long idx)
    {
        uint64_t z = (uint64_t) idx * 0x9E3779B97F4A7C15ull + (uint64_t) seed * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; z ^= z >> 31;
        return (double) (z >> 11) / 9007199254740992.0 * 2.0 - 1.0;
    }

    double randomValue()
    {
        if (p.rndMode == RndOff) return 0.0;
        // time base: host position when a seed is set and the host plays (instances stay in sync)
        const double t = (p.rndSeed != 0 && hostPlaying && hostSample >= 0) ? (double) (hostSample + blockSample) / fs : freeTime;
        const long long idx = (long long) std::floor (t * p.rndSpeed);
        if (idx != rndStepIndex)
        {
            rndStepIndex = idx;
            rndTarget = p.rndSeed != 0 ? hashUnit ((uint32_t) p.rndSeed, idx) : freeRng.next();
        }
        const double tau = std::max (0.0005, 0.5 * p.rndSmooth / std::max (0.1, p.rndSpeed));
        rndCur += (rndTarget - rndCur) * (1.0 - std::exp (-1.0 / (tau * fs)));
        return rndCur;
    }

    float readSinc (const retro::DelayBuf& b, double d) const
    {
        d = std::clamp (d, 16.0, (double) (b.mask - 20));
        const int i0 = (int) d;
        const double frac = d - i0;
        const int ph = (int) std::lround (frac * SincTable::kPhases);
        const float* k = &table().c[(size_t) (ph * SincTable::kTaps)];
        float s = 0.0f;
        for (int j = 0; j < SincTable::kTaps; ++j)
            s += k[j] * b.buf[(size_t) ((b.w - 1 - (i0 + j - 15)) & b.mask)];
        return s;
    }
    static const SincTable& table() { static const SincTable t; return t; }

    double fs = 48000.0, base = kBaseNormal, phase = 0.0, noiseGain = 0.0, satA = 0.513, satG = 1.0, freeTime = 0.0;
    double rndCur = 0.0, rndTarget = 0.0; long long rndStepIndex = -1;
    double fadeIn = 1.0, smoothK = 0.001, smWow = 0, smFlut = 0, smShape = 0, smStereo = 0, smOffset = 0, smWet = 0; bool smoothInit = false;
    bool hostPlaying = false; double hostPpq = 0.0, hostBpm = 120.0; long long hostSample = -1, blockSample = 0;
    Params p;
    std::array<retro::DelayBuf, 2> buf, dryBuf;
    std::array<TapeSat, 2> sat;
    std::array<Biquad, 2> ls, hs, pk, noiseLp;
    std::array<Rng, 2> noiseRng;
    Rng freeRng;
    double prevDelay[2] = { -1, -1 };
};
} // namespace wow
