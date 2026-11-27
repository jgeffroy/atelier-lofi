#pragma once

// VinylSim engine (JUCE-independent). Written from scratch.
//
// Physical chain, per channel:
//   cutting (lathe HF limiter, elliptical mono bass) -> warp (eccentric / warped record, delay modulation)
//   -> 2x oversampling -> RIAA recording curve -> groove displacement -> stylus tracing (spherical tip on the
//      groove wall, geometric model) -> velocity -> RIAA playback -> surface noise -> cartridge:
//      electrical RLC (coil L + DC resistance, load R || C) and stylus tip resonance (tip mass / vinyl
//      compliance) -> groove wear / translation losses -> 2x decimation
//   -> channel separation, subsonic filter, output, auto gain, phase-aligned dry/wet.
//
// "Brilliance" is the vinyl-specific top end: the cartridge resonance peak, the stylus tip resonance and the
// even-order tracing distortion ("sheen") that grows with frequency. Each can be pushed far beyond a real
// record with the x1 / x2 / x4 / x8 ranges. The tracing pair (tip on the groove and on its mirror image)
// separates the even-order distortion so it can be scaled without changing the linear response.

#include "../RetroDSP.h"
#include <array>

namespace vsim
{
using namespace retro;

struct CartDef   { const char* name; double L, Rc, tipHz, tipQ, extraLpHz; };
static const CartDef kCarts[] =
{
    { "MM Classic",        0.47,  625.0, 18000.0, 2.5,     0.0 },
    { "MM Bright",         0.40,  450.0, 15000.0, 3.5,     0.0 },
    { "MM Smooth",         0.50, 1100.0, 22000.0, 1.4,     0.0 },
    { "MM DJ",             0.65,  800.0, 14000.0, 2.0,     0.0 },
    { "Moving Coil",       1e-4,   10.0, 30000.0, 3.0,     0.0 },
    { "Ceramic (vintage)", 1.20, 2000.0,  9000.0, 4.0, 11000.0 },
};
struct StylusDef { const char* name; double radius, lossMul; };
static const StylusDef kStyli[] =
{
    { "Conical (18 um)",     18e-6, 1.00 },
    { "Elliptical (7 um)",    7e-6, 0.45 },
    { "Fine line (3 um)",     3e-6, 0.25 },
    { "Worn (28 um)",        28e-6, 1.60 },
};
struct FormatDef { const char* name; double rpm, rOuter, rInner, recHp, recLp, noiseDb; };
static const FormatDef kFormats[] =
{
    { "LP 33 (12 in)",      33.333, 0.146, 0.060,  0.0,     0.0, -66.0 },
    { "Maxi 45 (12 in)",    45.0,   0.146, 0.080,  0.0,     0.0, -68.0 },
    { "Single 45 (7 in)",   45.0,   0.084, 0.054,  0.0,     0.0, -62.0 },
    { "Shellac 78",         78.26,  0.146, 0.060, 90.0,  6500.0, -42.0 },
};

struct Params
{
    // brilliance
    double brilliance = 0.35; int brilRange = 0;          // 0..1, range index (x1, x2, x4, x8)
    int cart = 0; double loadPf = 200.0, loadKohm = 47.0;
    double tipRes = 0.5;                                    // 0..1 (amount of the tip resonance, 0.5 = natural)
    double sheen = 0.0; int sheenRange = 0;                 // extra even-order tracing distortion
    int stylus = 1, format = 0;
    double position = 0.3;                                  // 0 outer .. 1 inner groove
    double grooveDb = 0.0;                                  // cutting level
    double wear = 0.0;
    // cutting / playback
    double cutLimit = 0.3; int cutRange = 0;
    double ellipticalHz = 150.0;                            // 0 = off
    double separationDb = 25.0;
    double lowCutHz = 20.0;
    double warp = 0.15; int warpRange = 0; double flutter = 0.2;
    // surface
    double crackle = 0.25, dust = 0.25, pops = 0.1, hiss = 0.25, rumble = 0.2; int surfRange = 0;
    unsigned seed = 1;
    // output
    double inGainDb = 0.0, outGainDb = 0.0, mix = 1.0; bool autoGain = true, bypass = false;
};

inline double rangeMul (int r) { return (double) (1 << std::clamp (r, 0, 3)); }

// Bilinear transform of H(s) = (b2 s^2 + b1 s + b0) / (a2 s^2 + a1 s + a0), prewarped at fw (Hz)
inline void bilinear2 (Biquad& bq, double fs, double fw, double b2, double b1, double b0, double a2, double a1, double a0)
{
    fw = std::clamp (fw, 10.0, fs * 0.42);
    const double w = 2 * kPi * fw, K = w / std::tan (w / (2 * fs)), K2 = K * K;
    bq.set (b2 * K2 + b1 * K + b0, 2 * (b0 - b2 * K2), b2 * K2 - b1 * K + b0,
            a2 * K2 + a1 * K + a0, 2 * (a0 - a2 * K2), a2 * K2 - a1 * K + a0);
}

struct Riaa
{
    Biquad stage;
    void configure (double fs, bool playback)
    {
        const double t1 = 3180e-6, t2 = 318e-6, t3 = 75e-6, t4 = 3.18e-6;
        double nb2 = t2 * t4, nb1 = t2 + t4, na2 = t1 * t3, na1 = t1 + t3;
        if (! playback) { std::swap (nb2, na2); std::swap (nb1, na1); }
        bilinear2 (stage, fs, 1000.0, nb2, nb1, 1.0, na2, na1, 1.0);
        const double w = 2 * kPi * 1000.0 / fs;   // unity at 1 kHz
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = stage.b0 + stage.b1 * cr + stage.b2 * c2r, ni = stage.b1 * ci + stage.b2 * c2i;
        const double dr = 1.0 + stage.a1 * cr + stage.a2 * c2r, di = stage.a1 * ci + stage.a2 * c2i;
        const double g = std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
        stage.b0 /= g; stage.b1 /= g; stage.b2 /= g;
    }
    void reset() { stage.reset(); }
    double process (double x) { return stage.process (x); }
};

// Spherical tip of radius r riding on the groove wall. Where the groove curvature radius exceeds r, the tip
// centre follows the exact offset curve of the groove (contact point solved on a cubic interpolation of the
// groove, so the distortion is accurate below the sample grid). Where the groove is sharper than the tip,
// the centre is the upper envelope (mistracking). Fixed look-ahead of K samples (= latency).
struct Tracer
{
    static constexpr int K = 32, N = 128;
    std::array<double, N> ring {}; long long n = -1;
    std::array<double, 2 * K + 1> prof {}; int jmax = K;
    double r = 18e-6, dx = 5e-6, rdx = 4.0, lastShift = 0.0;
    void configure (double fs, double radius, double grooveSpeed)
    {
        r = radius; dx = grooveSpeed / fs; rdx = std::min ((double) K - 3.0, r / dx);
        jmax = 0;
        for (int j = -K; j <= K; ++j)
        {
            const double u = j * dx;
            const bool in = std::abs (u) < radius;
            prof[(size_t) (j + K)] = in ? std::sqrt (radius * radius - u * u) - radius : -1e9;
            if (in) jmax = std::max (jmax, std::abs (j));
        }
    }
    void reset() { ring.fill (0.0); n = -1; lastShift = 0.0; }
    double at (long long i) const { return ring[(size_t) (i & (N - 1))]; }
    // Catmull-Rom value and derivative (per sample) at fractional time t
    void interp (double t, double& v, double& d) const
    {
        const long long i = (long long) std::floor (t);
        const double f = t - (double) i;
        const double p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
        const double c1 = 0.5 * (p2 - p0), c2 = p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3, c3 = 0.5 * (p3 - p0) + 1.5 * (p1 - p2);
        v = ((c3 * f + c2) * f + c1) * f + p1;
        d = (3.0 * c3 * f + 2.0 * c2) * f + c1;
    }
    double process (double g)
    {
        ++n; ring[(size_t) (n & (N - 1))] = g;
        const double t0 = (double) (n - K);
        double t = t0 + lastShift, v = 0, d = 0, sl = 0;
        for (int it = 0; it < 6; ++it)
        {
            interp (t, v, d);
            sl = d / dx;
            const double tn = std::clamp (t0 + rdx * sl / std::sqrt (1.0 + sl * sl), t0 - K + 2.0, t0 + K - 3.0);
            const bool done = std::abs (tn - t) < 1e-4;
            t = tn;
            if (done) break;
        }
        lastShift = t - t0;
        interp (t, v, d); sl = d / dx;
        double y = v + r * (1.0 / std::sqrt (1.0 + sl * sl) - 1.0);
        for (int j = -jmax; j <= jmax; ++j)
            y = std::max (y, at (n - K + j) + prof[(size_t) (j + K)]);
        return y;
    }
};

// Linear-phase half-band FIR for 2x up/down sampling
struct HalfBand
{
    static constexpr int N = 63, D = 31;
    std::array<double, N> h {};
    HalfBand()
    {
        const double beta = 8.0;
        auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        double sum = 0;
        for (int n = 0; n < N; ++n)
        {
            const double m = n - D, x = 0.5 * m;
            const double sinc = m == 0 ? 0.5 : std::sin (kPi * x) / (kPi * m);
            const double r = m / D;
            h[(size_t) n] = sinc * i0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / i0 (beta);
            sum += h[(size_t) n];
        }
        for (auto& v : h) v /= sum;
        for (int k = 0; k < N; ++k) if (std::abs (h[(size_t) k]) > 1e-12) nz[(size_t) nnz++] = k;   // half-band: every other tap is zero
    }
    std::array<int, N> nz {}; int nnz = 0;
};
struct FirState
{
    std::array<double, 2 * HalfBand::N> z {}; int p = 0;
    void reset() { z.fill (0.0); p = 0; }
    void push (double x) { z[(size_t) p] = x; z[(size_t) p + HalfBand::N] = x; p = (p + 1) % HalfBand::N; }
    double out (const HalfBand& hb) const
    {
        double s = 0;   // z[p .. p+N-1] is oldest .. newest
        for (int k = 0; k < hb.nnz; ++k) { const int n = hb.nz[(size_t) k]; s += hb.h[(size_t) n] * z[(size_t) (p + HalfBand::N - 1 - n)]; }
        return s;
    }
};

struct Channel
{
    // base rate
    Biquad cutHp, sepHf, ellHp, lowCut1, lowCut2;
    Envelope cutEnv;
    DelayBuf warpBuf;
    FirState up, down;
    // high rate
    Riaa riaaRec, riaaPlay, noiseRiaa;
    Tracer trP, trN;
    Biquad cartE, cartTip, brilPk, extraLp, lossLp, recHp, recLp, wearHp, crackleBp, popLp, rumbleLp;
    double gPrev = 0, yPrev = 0;
    double popEnv = 0, popSign = 1;
    Rng rng;

    void reset()
    {
        for (auto* b : { &cutHp, &sepHf, &ellHp, &lowCut1, &lowCut2, &cartE, &cartTip, &brilPk, &extraLp, &lossLp,
                         &recHp, &recLp, &wearHp, &crackleBp, &popLp, &rumbleLp })
            b->reset();
        cutEnv.reset(); warpBuf.clear(); up.reset(); down.reset();
        riaaRec.reset(); riaaPlay.reset(); noiseRiaa.reset(); trP.reset(); trN.reset();
        gPrev = yPrev = 0; popEnv = 0;
    }
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; fh = 2 * fs;
        warpCenter = std::ceil (0.0065 * fs) + 4.0;
        for (auto& c : ch) { c.warpBuf.init ((int) (2 * warpCenter) + 16); }
        dryBuf[0].init (latencySamples() + 16); dryBuf[1].init (latencySamples() + 16);
        prepared = true;
        setParams (p, true);
        reset();
    }
    // total fixed latency (base-rate samples)
    int latencySamples() const { return (int) warpCenter + (2 * HalfBand::D + Tracer::K) / 2; }

    void reset()
    {
        for (int i = 0; i < 2; ++i) { ch[i].reset(); dryBuf[i].clear(); ch[i].rng = Rng(); }
        ch[0].rng.s = 0x9E3779B9u ^ (p.seed * 7919u); ch[1].rng.s = 0x7F4A7C15u ^ (p.seed * 104729u);
        shared.s = 0x2545F491u ^ (p.seed * 31u);
        rotPhase = 0; flutPhase = 0; rmsIn = rmsOut = 1e-6; agc = 1.0; fade = 0.0;
    }

    void setParams (const Params& np, bool force = false)
    {
        const bool structural = force || np.cart != p.cart || np.stylus != p.stylus || np.format != p.format || np.seed != p.seed;
        p = np;
        if (! prepared) return;
        const auto& cd = kCarts[std::clamp (p.cart, 0, 5)];
        const auto& sd = kStyli[std::clamp (p.stylus, 0, 3)];
        const auto& fd = kFormats[std::clamp (p.format, 0, 3)];

        // ---- brilliance: macro over cartridge peak, tip resonance and sheen
        const double B = std::clamp (p.brilliance, 0.0, 1.0) * rangeMul (p.brilRange);
        const double Bc = std::pow (B, 0.6);                         // 1 -> 1, 8 -> 3.5
        brilPkDb = 7.0 * Bc;                                          // x1 100 %: +7 dB, x8 100 %: +24 dB
        const double S = std::clamp (p.sheen, 0.0, 1.0) * rangeMul (p.sheenRange);
        sheenK = 1.0 + 5.0 * S + 0.6 * B;                             // even-order tracing distortion multiplier

        // ---- groove geometry
        const double rotHz = fd.rpm / 60.0;
        const double radius = fd.rOuter + (fd.rInner - fd.rOuter) * std::clamp (p.position, 0.0, 1.0);
        grooveSpeed = 2 * kPi * radius * rotHz;
        velScale = 0.2 * dbToGain (p.grooveDb);              // -12 dBFS at 1 kHz = 5 cm/s (reference cutting level)
        leak = 1.0 - 2 * kPi * 3.0 / fh;
        // translation / wavelength loss: grows towards the label, with the tip size and with wear
        const double lossHz = std::clamp (grooveSpeed / (sd.radius * sd.lossMul * 0.73 + 1e-12) * (1.0 - 0.6 * p.wear), 2500.0, 60000.0);   // conical inner groove ~16 kHz

        // ---- cartridge electrical model: H = R / (L R C s^2 + (L + Rc R C) s + Rc + R), unity at DC
        const double R = std::max (1000.0, p.loadKohm * 1000.0), C = std::max (10e-12, p.loadPf * 1e-12), L = cd.L, Rc = cd.Rc;
        const double f0 = std::sqrt ((Rc + R) / (L * R * C)) / (2 * kPi);
        cartHz = std::min (f0, 40000.0);
        const double tipDb = 7.0 * (std::clamp (p.tipRes, 0.0, 1.0) * 2.0 - 0.6) + 5.0 * Bc;   // natural ~ +3 dB

        // ---- cutting
        const double CL = std::clamp (p.cutLimit, 0.0, 1.0) * rangeMul (p.cutRange);
        cutThrDb = -6.0 - 9.0 * CL; cutMaxDb = std::min (36.0, 9.0 + 6.0 * CL); cutOn = CL > 0.001;
        sepC = dbToGain (-std::clamp (p.separationDb, 6.0, 60.0));

        // ---- warp / flutter (delay modulation, samples)
        const double Wm = std::clamp (p.warp, 0.0, 1.0) * rangeMul (p.warpRange);
        warpDepth = 0.0025 * Wm / (2 * kPi * rotHz) * fs;            // x1 100 %: 0.25 % pitch, x8: 2 %
        warpDepth = std::min (warpDepth, warpCenter - 3.0);
        flutDepth = 0.0006 * std::clamp (p.flutter, 0.0, 1.0) * (1.0 + 0.3 * Wm) / (2 * kPi * 8.0) * fs;
        rotInc = rotHz / fs;

        // ---- surface
        const double SR = rangeMul (p.surfRange);
        crackleRate = 2200.0 * std::pow (std::clamp (p.crackle, 0.0, 1.0), 1.3) * std::sqrt (SR);
        crackleAmp  = 0.35 * p.crackle * std::sqrt (SR);
        dustRate = 9000.0 * p.dust * std::sqrt (SR); dustAmp = 0.05 * p.dust * std::sqrt (SR);
        popRate = 25.0 * p.pops * p.pops * SR; popAmp = 0.9 * p.pops * std::sqrt (SR);
        hissGain = p.hiss <= 0.0 ? 0.0 : dbToGain (fd.noiseDb + 30.0 * std::log10 (p.hiss * SR + 1e-9) + 10.0) * 0.6;
        rumbleGain = p.rumble <= 0.0 ? 0.0 : dbToGain (-46.0 + 20.0 * std::log10 (p.rumble * SR)) * 6.0;
        popDecay = std::exp (-1.0 / (0.004 * fh));

        for (auto& c : ch)
        {
            c.cutHp.highpass (fs, 4500.0, 0.7);
            c.sepHf.highpass (fs, 6000.0, 0.6);
            if (p.ellipticalHz > 1.0) c.ellHp.highpass (fs, p.ellipticalHz, 0.7071);
            c.lowCut1.highpass (fs, std::max (5.0, p.lowCutHz), 0.54);
            c.lowCut2.highpass (fs, std::max (5.0, p.lowCutHz), 1.31);
            if (structural)
            {
                c.riaaRec.configure (fh, false); c.riaaPlay.configure (fh, true); c.noiseRiaa.configure (fh, true);
            }
            c.trP.configure (fh, sd.radius, grooveSpeed); c.trN.configure (fh, sd.radius, grooveSpeed);
            bilinear2 (c.cartE, fh, cartHz, 0, 0, R / (Rc + R), L * R * C / (Rc + R), (L + Rc * R * C) / (Rc + R), 1.0);
            c.cartTip.peak (fh, std::min (cd.tipHz * (1.0 + 0.15 * (sd.radius < 10e-6 ? 1 : 0)), fh * 0.42), cd.tipQ, tipDb);
            c.brilPk.peak (fh, std::clamp (cartHz, 6000.0, fh * 0.4), 1.1, brilPkDb);
            if (cd.extraLpHz > 0) c.extraLp.lowpass (fh, cd.extraLpHz, 0.8); else c.extraLp.set (1, 0, 0, 1, 0, 0);
            c.lossLp.lowpass (fh, std::min (lossHz, fh * 0.42), 0.6);
            if (fd.recHp > 0) c.recHp.highpass (fh, fd.recHp, 0.7); else c.recHp.set (1, 0, 0, 1, 0, 0);
            if (fd.recLp > 0) c.recLp.lowpass (fh, fd.recLp, 0.9); else c.recLp.set (1, 0, 0, 1, 0, 0);
            c.wearHp.highpass (fh, 2500.0, 0.7);
            c.crackleBp.bandpass (fh, 3500.0, 0.9);
            c.popLp.lowpass (fh, 1500.0, 0.7);
            c.rumbleLp.lowpass (fh, 28.0, 0.7);
            c.cutEnv.set (fs, 0.0005, 0.03);
        }
        wearAmt = std::clamp (p.wear, 0.0, 1.0);
        inGain = dbToGain (p.inGainDb); outGain = dbToGain (p.outGainDb);
        if (structural) fade = 0.0;
    }

    void process (double& l, double& r)
    {
        double in[2] = { l * inGain, r * inGain };
        // aligned dry
        double dry[2];
        for (int c = 0; c < 2; ++c) { dryBuf[c].write ((float) in[c]); dry[c] = dryBuf[c].read ((double) latencySamples()); }
        if (p.bypass) { l = dry[0] / inGain; r = dry[1] / inGain; return; }

        // ---- cutting: elliptical filter (mono bass) and lathe HF limiter
        double x[2] = { in[0], in[1] };
        if (p.ellipticalHz > 1.0)
        {
            const double m = 0.5 * (x[0] + x[1]), s = ch[0].ellHp.process (0.5 * (x[0] - x[1]));
            x[0] = m + s; x[1] = m - s;
        }
        if (cutOn)
            for (int c = 0; c < 2; ++c)
            {
                const double h = ch[c].cutHp.process (x[c]);
                const double L = gainToDb (ch[c].cutEnv.process (h) * 2.0 + 1e-9);   // ~ pre-emphasised level
                const double red = std::min (cutMaxDb, std::max (0.0, L - cutThrDb) * 0.8);
                x[c] += (dbToGain (-red) - 1.0) * h;
            }

        // ---- warp and flutter
        rotPhase += rotInc; if (rotPhase >= 1) rotPhase -= 1;
        flutPhase += 8.0 / fs; if (flutPhase >= 1) flutPhase -= 1;
        const double ph = 2 * kPi * rotPhase;
        const double d = warpCenter + warpDepth * (std::sin (ph) + 0.25 * std::sin (2 * ph + 0.7))
                       + flutDepth * (std::sin (2 * kPi * flutPhase) + 0.5 * std::sin (2 * kPi * flutPhase * 2.7 + 1.0));
        for (int c = 0; c < 2; ++c) { ch[c].warpBuf.write ((float) x[c]); x[c] = ch[c].warpBuf.read (d); }

        // ---- correlated surface events (both walls)
        double sc[2] = { 0, 0 };
        double sharedClick = 0, sharedPop = 0;

        // ---- 2x oversampled record / playback
        double y[2];
        for (int c = 0; c < 2; ++c)
        {
            auto& C = ch[c];
            double hi[2];
            C.up.push (x[c] * 2.0); hi[0] = C.up.out (hb);
            C.up.push (0.0);        hi[1] = C.up.out (hb);
            for (int k = 0; k < 2; ++k)
            {
                if (c == 0)
                {
                    sharedClick = shared.uniform() < 0.5 * crackleRate / fh ? crackleAmp * std::pow (shared.uniform(), 4.0) * (shared.next() > 0 ? 1 : -1) : 0.0;
                    sharedPop = shared.uniform() < 0.3 * popRate / fh ? popAmp * (0.4 + 0.6 * shared.uniform()) * (shared.next() > 0 ? 1 : -1) : 0.0;
                    sh[k][0] = sharedClick; sh[k][1] = sharedPop;
                }
                C.down.push (hiRate (C, hi[k], sh[k][0], sh[k][1]));
                if (k == 0) y[c] = C.down.out (hb);   // output at even high-rate index: latency (2D + K) / 2 exactly
            }
        }
        (void) sc;

        // ---- channel separation (crosstalk grows with frequency), subsonic filter
        {
            double m = 0.5 * (y[0] + y[1]), s = 0.5 * (y[0] - y[1]);
            const double cLow = sepC, cHigh = std::min (0.6, sepC * 3.5);
            const double sh2 = ch[0].sepHf.process (s);
            s = s * (1 - cLow) / (1 + cLow) + sh2 * ((1 - cHigh) / (1 + cHigh) - (1 - cLow) / (1 + cLow));
            y[0] = m + s; y[1] = m - s;
        }
        for (int c = 0; c < 2; ++c) y[c] = ch[c].lowCut2.process (ch[c].lowCut1.process (y[c]));

        // ---- auto gain (slow RMS match against the input)
        if (p.autoGain)
        {
            const double a = 1.0 / (1.5 * fs);
            rmsIn  += (0.5 * (dry[0] * dry[0] + dry[1] * dry[1]) - rmsIn) * a;
            rmsOut += (0.5 * (y[0] * y[0] + y[1] * y[1]) - rmsOut) * a;
            const double target = std::clamp (std::sqrt ((rmsIn + 1e-9) / (rmsOut + 1e-9)), 0.25, 2.0);
            agc += (target - agc) * (1.0 / (0.5 * fs));
        }
        else agc = 1.0;

        if (fade < 1.0) fade = std::min (1.0, fade + 1.0 / (0.03 * fs));
        for (int c = 0; c < 2; ++c)
        {
            double w = y[c] * agc * fade;
            double o = (dry[c] + (w - dry[c]) * std::clamp (p.mix, 0.0, 1.0)) * outGain;
            // transparent safety clip above -1 dBFS
            const double a = std::abs (o);
            if (a > 0.89) o = (o > 0 ? 1 : -1) * (0.89 + 0.11 * std::tanh ((a - 0.89) / 0.11));
            (c == 0 ? l : r) = o;
        }
    }

    // playback-chain magnitude at f (cartridge, tip, brilliance, losses), for the UI curve
    double magnitudeDb (double f) const
    {
        const auto& C = ch[0];
        const double w = 2 * kPi * f / fh, cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        auto mag = [&] (const Biquad& b)
        {
            const double nr = b.b0 + b.b1 * cr + b.b2 * c2r, ni = b.b1 * ci + b.b2 * c2i;
            const double dr = 1.0 + b.a1 * cr + b.a2 * c2r, di = b.a1 * ci + b.a2 * c2i;
            return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30);
        };
        double db = mag (C.cartE) + mag (C.cartTip) + mag (C.extraLp) + mag (C.lossLp) + mag (C.recHp) + mag (C.recLp);
        if (brilPkDb > 0.05) db += mag (C.brilPk);
        return db;
    }
    // accessors for tests / UI
    double cartResonanceHz() const { return cartHz; }
    double grooveSpeedMs() const { return grooveSpeed; }
    double sheenMultiplier() const { return sheenK; }
    double brilliancePeakDb() const { return brilPkDb; }

private:
    double hiRate (Channel& C, double x, double sharedClick, double sharedPop)
    {
        // record: RIAA cut -> groove velocity -> displacement
        const double vel = C.riaaRec.process (x) * velScale;
        const double g = C.gPrev * leak + vel / fh;
        C.gPrev = g;
        // tip on the groove and on its mirror: even-order distortion = half sum
        const double yp = C.trP.process (g), yn = C.trN.process (-g);
        const double yo = 0.5 * (yp - yn), ye = 0.5 * (yp + yn);
        const double yc = yo + sheenK * ye;
        double v = (yc - C.yPrev) * fh;
        C.yPrev = yc;
        double s = C.riaaPlay.process (v / velScale);

        // groove wear: hard grit on the highs
        if (wearAmt > 0.0) { const double h = C.wearHp.process (s); s += wearAmt * 3.0 * h * std::abs (h); }

        // surface noise (in the groove, so it goes through the cartridge)
        double surf = 0;
        if (C.rng.uniform() < 0.5 * crackleRate / fh) surf += crackleAmp * std::pow (C.rng.uniform(), 4.0) * (C.rng.next() > 0 ? 1 : -1);
        if (C.rng.uniform() < dustRate / fh) surf += dustAmp * std::pow (C.rng.uniform(), 3.0) * (C.rng.next() > 0 ? 1 : -1);
        surf += sharedClick;
        surf = C.crackleBp.process (surf) * 1.6 + surf * 0.5;
        if (C.rng.uniform() < 0.7 * popRate / fh) { C.popEnv = popAmp * (0.3 + 0.7 * C.rng.uniform()); C.popSign = C.rng.next() > 0 ? 1 : -1; }
        if (sharedPop != 0.0) { C.popEnv = std::max (C.popEnv, std::abs (sharedPop)); C.popSign = sharedPop > 0 ? 1 : -1; }
        surf += C.popLp.process (C.popEnv * (0.7 * C.popSign + 0.3 * C.rng.next())) * 2.0;
        C.popEnv *= popDecay;
        if (hissGain > 0) surf += C.noiseRiaa.process (C.rng.next()) * hissGain * 6.0;
        if (rumbleGain > 0) surf += C.rumbleLp.process (C.rng.next()) * rumbleGain;
        s += surf;

        // cartridge and losses
        s = C.cartE.process (s);
        s = C.cartTip.process (s);
        if (brilPkDb > 0.05) s = C.brilPk.process (s);
        s = C.extraLp.process (s);
        s = C.lossLp.process (s);
        s = C.recLp.process (C.recHp.process (s));
        return s;
    }

    Params p;
    bool prepared = false;
    double fs = 48000, fh = 96000;
    HalfBand hb;
    Channel ch[2];
    DelayBuf dryBuf[2];
    Rng shared;
    double sh[2][2] {};
    double warpCenter = 316, warpDepth = 0, flutDepth = 0, rotInc = 0, rotPhase = 0, flutPhase = 0;
    double grooveSpeed = 0.5, velScale = 0.05, leak = 0.9998, sheenK = 1, brilPkDb = 0, cartHz = 16000;
    double cutThrDb = -12, cutMaxDb = 12; bool cutOn = false;
    double sepC = 0.05, wearAmt = 0;
    double crackleRate = 0, crackleAmp = 0, dustRate = 0, dustAmp = 0, popRate = 0, popAmp = 0, popDecay = 0.99;
    double hissGain = 0, rumbleGain = 0;
    double inGain = 1, outGain = 1, rmsIn = 1e-6, rmsOut = 1e-6, agc = 1, fade = 1;
};
} // namespace vsim
