#pragma once

// RetroDeck 2 engine: cassette, vinyl and VHS emulation (original DSP, independent of JUCE).
// Version 2 uses published physical models: Jiles-Atherton magnetic hysteresis for tape (as in the
// DAFx 2019 tape modelling literature) and the geometric stylus tracing model for vinyl (tracing is
// documented as the main source of record playback distortion). Effects also have a wider range.
//
// Signal flow (per sample, stereo):
//   input gain -> media "colour" (record electronics, medium, playback electronics; repeated per dub
//   generation for cassette/VHS) -> transport (wow, flutter, drift, snaps, warp, stop/start)
//   -> damage (dropouts, wear, VHS tracking, head switching) -> hum / motor -> crosstalk, width
//   -> print-through -> echo -> tone EQ -> auto gain / output -> dry/wet mix

#include "../RetroDSP.h"
namespace retro2 { using namespace retro; }
#include <array>

namespace retro2
{
enum Media { MediaCassette = 0, MediaVinyl, MediaVhs };

// ----------------------------------------------------------------------------------------------
// Characteristic tables (original values chosen to reflect the documented behaviour of each format)
struct DeckDef { double hfMul, bumpDb, bumpHz, wowPct, flutPct, hissDb, xtalkDb, lowCutHz, driftMul; };
static const DeckDef kDecks[] =
{   //  hfMul bump  bumpHz wow%  flut%  hiss  xtalk  lowcut drift
    { 1.30, 1.5,  55.0, 0.04, 0.03, -3.0, -45.0,  20.0, 0.3 },   // Studio 3-head
    { 1.05, 3.0,  65.0, 0.12, 0.10,  0.0, -38.0,  30.0, 0.6 },   // Portastudio 4-track
    { 1.00, 2.0,  60.0, 0.12, 0.08,  0.0, -32.0,  30.0, 0.6 },   // Home stereo deck
    { 0.80, 2.5,  60.0, 0.45, 0.25,  2.0, -28.0,  40.0, 1.5 },   // Walkman
    { 0.70, 4.0,  70.0, 0.30, 0.20,  3.0, -25.0,  60.0, 1.0 },   // Boombox
    { 0.30, 1.0, 120.0, 0.60, 0.50,  7.0, -20.0, 200.0, 1.5 },   // Dictaphone
};
struct TapeDef { double hfHz, hissDb, drive, asym, hfSens, emphHz; };
static const TapeDef kTapes[] =
{   //  hf(1 7/8)  hiss    drive asym  hfSens emph
    {  8000.0, -50.0, 1.25, 0.10, 1.25, 1326.0 },   // Type I ferric
    { 12000.0, -55.0, 1.00, 0.06, 0.90, 2274.0 },   // Type II chrome
    { 11000.0, -54.0, 0.95, 0.07, 1.00, 2274.0 },   // Type III ferro-chrome
    { 14500.0, -56.0, 0.75, 0.04, 0.60, 2274.0 },   // Type IV metal
};
struct RecordDef { double rpm, bwLow, bwHigh, noiseDb, xtalkDb, drive; bool mono; };
static const RecordDef kRecords[] =
{
    { 33.333,  20.0, 18000.0, -62.0, -28.0, 1.0, false },   // LP 33
    { 45.0,    20.0, 20000.0, -64.0, -30.0, 0.9, false },   // single 45
    { 78.0,   120.0,  5500.0, -38.0, -99.0, 1.4, true  },   // shellac 78
    { 33.333,  15.0, 20000.0, -66.0, -32.0, 0.8, false },   // lacquer / dubplate
};
struct TableDef { double rumbleDb, wowPct, flutPct, lpHz, hpHz, midDb; };
static const TableDef kTables[] =
{
    { -6.0, 0.02, 0.015, 22000.0,  15.0, 0.0 },   // studio deck
    { -3.0, 0.04, 0.020, 22000.0,  18.0, 0.0 },   // DJ direct drive
    {  6.0, 0.25, 0.120,  8000.0, 120.0, 3.0 },   // portable suitcase player
    { 10.0, 0.40, 0.200,  3800.0, 250.0, 4.0 },   // gramophone
};
struct VhsSpeedDef { double bwLow, bwHigh, hissDb, wowPct, flutPct; };
static const VhsSpeedDef kVhsSpeeds[] =
{
    {  70.0, 10000.0, -48.0, 0.12, 0.10 },   // SP
    {  90.0,  7000.0, -45.0, 0.18, 0.14 },   // LP
    { 100.0,  5000.0, -42.0, 0.25, 0.20 },   // EP
};

// ----------------------------------------------------------------------------------------------
struct Params
{
    static constexpr int kLatency = 2 + 16;   // transport offset + stylus look-ahead
    double fs = 48000.0;
    int    media = MediaCassette;
    double inGainDb = 0, outGainDb = 0, mix = 1, drive = 0.3, width = 1, lowCut = 20, highCut = 22000, tone = 0;
    bool   autoGain = true, mono = false, secResp = true, secSat = true, secComp = true;
    // stage amounts (1 = nominal; up to 8 with the x2/x4/x8 ranges) and response editor
    double respAmt = 1, satAmt = 1, compAmt = 1, hfTrimOct = 0, lowTrimOct = 0, bumpMul = 1;
    int    hfSlope = 1;            // 0: 12 dB/oct, 1: 24 dB/oct, 2: 36 dB/oct
    int    mixMode = 0;            // 0: single path (mix handled as morph), 1: parallel aligned, 2: parallel classic

    // cassette
    int    cDeck = 2, cTape = 1, cSpeed = 0, cSatModel = 0, cAlc = 0, cNr = 0, cNrMode = 0, cGen = 1;
    double cBias = 0, cHfComp = 0.5, cAlcAmt = 0.5, cNrTrack = 0, cAzimuth = 0, cXtalkOffset = 0,
           cPrintDb = -80, cPrintTime = 1.4, cBumpDb = 0, cAsperity = 0.3;

    // vinyl
    int    vTable = 0, vCart = 1, vRecord = 0, vMonoBass = 0, vStylus = 0;
    double vPos = 0.3, vWear = 0.1, vCrackle = 0.2, vPops = 0.1, vDust = 0.2, vScratch = 0, vWarp = 0.1,
           vEcc = 0.1, vRumbleDb = -70, vCutLimit = 0, vCrackleTone = 0.5;

    // VHS
    int    hFormat = 2, hSpeed = 0, hStandard = 0, hGen = 1;
    double hTracking = 0.1, hSwitch = 0.2, hBleedDb = -70, hCompTrack = 0;
    bool   hDolby = true;

    // motion
    double wfAmount = 1, wowDepth = 1, wowRate = 0.6, flutDepth = 1, flutRate = 10, wfRandom = 0.3,
           drift = 0.2, snap = 0, fm = 0, wowSyncHz = 0, stopTime = 0.8, startTime = 0.4;
    bool   stop = false;

    // noise and damage
    double hissDb = 0, humDb = -90, humHz = 50, motorDb = -90, age = 0, dropouts = 0, dropRate = 1,
           dropWidth = 0.3, degrade = 0, degradeRate = 1;
    bool   hissGate = false;

    // echo
    int    echoType = 0;
    double echoMs = 180, echoFb = 0.35, echoLevel = 0.35, echoHP = 150, echoLP = 6000;
};

// Moves a corner frequency from "flat" (fFlat) to its nominal value f as amount goes 0 -> 1,
// and beyond it for amounts > 1 (log interpolation, so the result is musical).
inline double morphHz (double fFlat, double f, double amount, double lo, double hi)
{
    // above 100 % the extra amount is progressive, so x2 / x4 / x8 stay musical
    const double a = amount <= 1.0 ? amount : 1.0 + (amount - 1.0) * 0.35;
    return std::clamp (std::exp (std::log (fFlat) + a * (std::log (f) - std::log (fFlat))), lo, hi);
}

// ----------------------------------------------------------------------------------------------
// Noise reduction compander: B-type and C-type (sliding high-frequency shelf), dbx-type and
// VHS Hi-Fi type (wide-band compander with emphasis). Encode and decode are separate instances.
struct NoiseReduction
{
    enum Type { Off = 0, BType, CType, Dbx, HiFi };
    int type = Off; double fs = 48000;
    Biquad hp, emph; Envelope env; double ms = 0, rmsK = 0.001;
    double maxB = 10, ref = -30, span = 20, ratio = 2;

    void configure (double sampleRate, int t, bool decoder)
    {
        const bool typeChanged = t != type;
        fs = sampleRate; type = t;
        if (t == BType)      { maxB = 10; ref = -15; span = 25; hp.highpass (fs, 1500.0, 0.6); }
        else if (t == CType) { maxB = 18; ref = -15; span = 30; hp.highpass (fs, 700.0, 0.6); }
        env.set (fs, 0.002, 0.05);
        ratio = t == HiFi ? 2.5 : 2.0;
        ref = (t == Dbx || t == HiFi) ? -24.0 : ref;
        emph.highShelf (fs, 2000.0, decoder ? -6.0 : 6.0);
        rmsK = 1.0 - std::exp (-1.0 / ((t == HiFi ? 0.012 : 0.025) * fs));
        if (typeChanged) reset();   // a new compander type starts at unity gain
    }
    void reset() { hp.reset(); emph.reset(); env.reset(); ms = std::pow (10.0, ref / 10.0); }   // start at the reference level (unity gain)

    double encode (double x)
    {
        if (type == BType || type == CType)
        {
            const double h = hp.process (x);
            const double L = gainToDb (env.process (h) + 1e-9);
            const double g = maxB * std::clamp ((ref - L) / span, 0.0, 1.0);
            return x + (dbToGain (g) - 1.0) * h;
        }
        if (type == Dbx || type == HiFi)
        {
            const double e = emph.process (x);
            ms += (e * e - ms) * rmsK;
            const double L = 10.0 * std::log10 (ms + 1e-12);
            const double g = std::clamp ((ref - L) * (1.0 - 1.0 / ratio), -30.0, 30.0);
            return e * dbToGain (g);
        }
        return x;
    }

    double decode (double y, double trackDb)
    {
        if (type == BType || type == CType)
        {
            const double h = hp.process (y);
            const double Lenc = gainToDb (env.process (h) + 1e-9) - trackDb;
            double L;
            if (Lenc >= ref) L = Lenc;
            else if (Lenc <= ref - span + maxB) L = Lenc - maxB;
            else L = (Lenc - maxB * ref / span) / (1.0 - maxB / span);
            const double g = maxB * std::clamp ((ref - L) / span, 0.0, 1.0);
            return y + (dbToGain (-g) - 1.0) * h;
        }
        if (type == Dbx || type == HiFi)
        {
            // feedback detection: the decoder measures its own (decoded) output, mirroring the encoder,
            // so tracking also holds on transients. Mistracking = expansion ratio error, which makes
            // loud and quiet passages "breathe" around the reference level.
            const double Lz = 10.0 * std::log10 (ms + 1e-12);
            const double rDec = ratio * (1.0 + trackDb / 20.0);
            const double g = std::clamp ((Lz - ref) * (1.0 - 1.0 / rDec), -30.0, 30.0);
            const double z = y * dbToGain (g);
            ms += (z * z - ms) * rmsK;
            return emph.process (z);
        }
        return y;
    }
};

// ----------------------------------------------------------------------------------------------
// Jiles-Atherton magnetic hysteresis, solved with RK2 at twice the sample rate.
// Parameters follow the usual tape values (Ms, a, alpha, k, c); bias acts on k and c.
struct Hysteresis
{
    double Ms = 3.5e5, a = 2.2e4, alpha = 1.6e-3, k = 2.7e4, c = 0.17;
    double hScale = 5.0e4, norm = 1.0, M = 0.0, Hprev = 0.0;

    static double langevin (double x)  { return std::abs (x) < 1e-3 ? x / 3.0 - x * x * x / 45.0 : 1.0 / std::tanh (x) - 1.0 / x; }
    static double langevinD (double x) { if (std::abs (x) < 1e-3) return 1.0 / 3.0 - x * x / 15.0; const double sh = std::sinh (x); return 1.0 / (x * x) - 1.0 / (sh * sh); }

    double dMdH (double m, double H, double delta) const
    {
        const double Q = (H + alpha * m) / a;
        const double man = Ms * langevin (Q), lp = langevinD (Q);
        const double diff = man - m;
        const double deltaM = ((delta > 0) == (diff > 0)) ? 1.0 : 0.0;
        const double den1 = (1.0 - c) * delta * k - alpha * diff;
        // the irreversible term is only valid while its denominator keeps the sign of delta;
        // near the singularity it is dropped (avoids numerical spikes on noisy, fast signals)
        const double t1 = (den1 * delta > 0.05 * k) ? (1.0 - c) * deltaM * diff / den1 : 0.0;
        const double t2 = c * (Ms / a) * lp;
        const double den = 1.0 - c * alpha * (Ms / a) * lp;
        const double slope = (t1 + t2) / (std::abs (den) > 1e-9 ? den : 1e-9);
        return std::clamp (slope, 0.0, 4.0 * Ms / a);   // dM/dH is physically positive and bounded
    }

    void step (double H0, double H1)
    {
        const double dH = H1 - H0;
        if (std::abs (dH) < 1e-12) return;
        const double delta = dH > 0 ? 1.0 : -1.0;
        const double k1 = dMdH (M, H0, delta) * dH;
        const double k2 = dMdH (M + 0.5 * k1, 0.5 * (H0 + H1), delta) * dH;
        M = std::clamp (M + k2, -Ms, Ms);
    }

    // AC bias makes real tape recording close to anhysteretic: the output blends the anhysteretic
    // curve with the hysteresis loop; the blend (loopAmt) follows the bias setting.
    double asym = 0.0, loopAmt = 0.25, aEff = 2.2e4;
    double process (double x)
    {
        const double H = (x + asym) * hScale;
        const double Hmid = 0.5 * (Hprev + H);
        step (Hprev, Hmid);
        step (Hmid, H);
        Hprev = H;
        const double anh = Ms * langevin (H / aEff);
        return ((1.0 - loopAmt) * anh + loopAmt * M) / Ms * norm;
    }

    void reset() { M = 0.0; Hprev = 0.0; }

    // drive: field scale; bias -1 (under) .. +1 (over); computes a small-signal normalisation
    void configure (double drive, double bias)
    {
        const double k0 = 2.7e4, c0 = 0.45;   // higher reversibility = the linearising effect of AC bias
        k = k0 * (bias < 0 ? 1.0 + 0.9 * (-bias) : 1.0 - 0.5 * bias);
        c = std::clamp (c0 * (bias < 0 ? 1.0 - 0.6 * (-bias) : 1.0 + 2.0 * bias), 0.02, 0.9);
        hScale = 2.0e4 * drive;
        loopAmt = std::clamp (0.25 + 0.55 * std::max (0.0, -bias) - 0.2 * std::max (0.0, bias), 0.05, 0.8);
        aEff = std::max (1000.0, a - alpha * Ms / 3.0);   // same small-signal slope as the coupled model
        // normalise at the reference level (-12 dBFS peak): more drive gives more colour and
        // compression, not a quieter output
        Hysteresis t = *this; t.norm = 1.0; t.reset();
        double peak = 0.0;
        for (int i = 0; i < 800; ++i)
        {
            const double v = t.process (0.25 * std::sin (2 * kPi * i / 40.0));
            if (i >= 400) peak = std::max (peak, std::abs (v));
        }
        norm = peak > 1e-12 ? 0.25 / peak : 1.0;
    }
};

// RIAA recording pre-emphasis and playback de-emphasis (time constants 3180 / 318 / 75 us),
// bilinear transform of the analog prototypes.
struct Riaa
{
    Biquad stage;
    OnePoleLP dummy;
    void configure (double fs, bool playback)
    {
        const double t1 = 3180e-6, t2 = 318e-6, t3 = 75e-6, t4 = 3.18e-6;
        // playback: (1 + s t2)(1 + s t4) / ((1 + s t1)(1 + s t3)); recording is the inverse
        double nb2 = t2 * t4, nb1 = t2 + t4, nb0 = 1.0;
        double na2 = t1 * t3, na1 = t1 + t3, na0 = 1.0;
        if (! playback) { std::swap (nb2, na2); std::swap (nb1, na1); std::swap (nb0, na0); }
        const double K = 2.0 * fs, K2 = K * K;
        const double b0 = nb2 * K2 + nb1 * K + nb0, b1 = 2.0 * (nb0 - nb2 * K2), b2 = nb2 * K2 - nb1 * K + nb0;
        const double a0 = na2 * K2 + na1 * K + na0, a1 = 2.0 * (na0 - na2 * K2), a2 = na2 * K2 - na1 * K + na0;
        stage.set (b0, b1, b2, a0, a1, a2);
        // normalise to unity gain at 1 kHz
        const double w = 2 * kPi * 1000.0 / fs;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = stage.b0 + stage.b1 * cr + stage.b2 * c2r, ni = stage.b1 * ci + stage.b2 * c2i;
        const double dr = 1.0 + stage.a1 * cr + stage.a2 * c2r, di = stage.a1 * ci + stage.a2 * c2i;
        const double g = std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
        stage.b0 /= g; stage.b1 /= g; stage.b2 /= g;
    }
    void reset() { stage.reset(); }
    double process (double x) { return stage.process (x); }
};

// Geometric stylus tracing: a spherical tip of radius r rides on the groove wall; its centre is the
// upper envelope of the wall plus the tip profile. The groove speed shrinks towards the label,
// so the same wavelength becomes shorter and the distortion grows on the inner grooves.
struct Tracer
{
    static constexpr int K = 16;                       // look-ahead (samples), gives a fixed latency of K
    std::array<double, 64> ring {}; int w = 0;
    std::array<double, 2 * K + 1> prof {};             // tip profile per neighbouring sample (or -inf)

    void configure (double fs, double radius, double grooveSpeed)
    {
        const double dx = grooveSpeed / fs;
        for (int j = -K; j <= K; ++j)
        {
            const double u = j * dx;
            prof[(size_t) (j + K)] = std::abs (u) < radius ? std::sqrt (radius * radius - u * u) - radius : -1e9;
        }
    }
    void reset() { ring.fill (0.0); w = 0; }

    double process (double g)
    {
        ring[(size_t) w] = g; w = (w + 1) & 63;
        // centre position: K samples ago (the window reaches K samples into the "future")
        double best = -1e9;
        for (int j = -K; j <= K; ++j)
        {
            const double pj = prof[(size_t) (j + K)];
            if (pj < -1e8) continue;
            const double gj = ring[(size_t) ((w - 1 - K + j) & 63)];
            best = std::max (best, gj + pj);
        }
        return best;
    }
};

// ----------------------------------------------------------------------------------------------
// One record/playback pass on magnetic tape (cassette or VHS linear / Hi-Fi). Repeated for dubs.
struct TapePassConfig
{
    int nrType = 0; bool nrEncode = true, nrDecode = true; double nrTrack = 0;
    double emphHz = 2274, emphDb = 0, biasPreDb = 0;
    bool satOn = true, satHard = false; double satBias = 0.05, drive = 1.0;
    bool hysteresis = true; double hystBias = 0.0;
    double hk = 2.7e4, hc = 0.17, hScale = 5e4, hNorm = 1.0;   // shared, precomputed hysteresis settings
    bool compOn = true; double hfCompAmt = 0.5, hfSens = 1.0;
    double hissGain = 0, asperity = 0;
    bool respOn = true; double lpHz = 15000, hpHz = 30, bumpHz = 60, bumpDb = 0;
    int slope = 1; double compCap = 14.0;
};

struct TapePass
{
    TapePassConfig cfg;
    NoiseReduction enc, dec;
    Biquad pre, de, biasPre, lp1, lp2, lp3, bump, hp, hfHp, aspBp;
    AdaaSat sat; DcBlocker dc; Envelope hfEnv, aspEnv; Hysteresis hyst; double adaaComp = 1.0;
    Rng rng; double fs = 48000;

    void configure (double sampleRate, const TapePassConfig& c)
    {
        fs = sampleRate; cfg = c;
        enc.configure (fs, c.nrType, false);
        dec.configure (fs, c.nrType, true);
        pre.highShelf (fs, c.emphHz, c.emphDb);
        de.highShelf (fs, c.emphHz, -c.emphDb);
        if (c.biasPreDb > 0.01) biasPre.highShelf (fs, 6000.0, c.biasPreDb); else biasPre.identity();
        if (c.slope == 0)      { lp1.lowpass (fs, c.lpHz, 0.7071); lp2.identity(); lp3.identity(); }
        else if (c.slope == 1) { lp1.lowpass (fs, c.lpHz, 0.54);   lp2.lowpass (fs, c.lpHz * 1.15, 1.25); lp3.identity(); }
        else                   { lp1.lowpass (fs, c.lpHz, 0.5176); lp2.lowpass (fs, c.lpHz, 0.7071); lp3.lowpass (fs, c.lpHz * 1.05, 1.9319); }
        hp.highpass (fs, c.hpHz, 0.7);
        bump.peak (fs, c.bumpHz, 1.0, c.bumpDb);
        hfHp.highpass (fs, 5000.0, 0.7);
        aspBp.bandpass (fs, 3000.0, 0.5);
        dc.set (fs);
        hyst.k = c.hk; hyst.c = c.hc; hyst.hScale = c.hScale; hyst.norm = c.hNorm;   // keeps the magnetic state
        hyst.asym = c.hysteresis ? 0.25 * c.satBias : 0.0;                          // slight asymmetry (even harmonics)
        {
            // level compensation for the static curves at the -12 dBFS reference
            const double ref = 0.25, y = c.satHard ? AdaaSat::fHard (ref * c.drive) : AdaaSat::fSmooth (ref * c.drive, c.satBias);
            adaaComp = std::abs (y) > 1e-9 ? ref * c.drive / std::abs (y) : 1.0;
        }
        hfEnv.set (fs, 0.001, 0.08);
        aspEnv.set (fs, 0.005, 0.05);
    }
    void reset()
    {
        enc.reset(); dec.reset(); pre.reset(); de.reset(); biasPre.reset(); lp1.reset(); lp2.reset(); lp3.reset(); bump.reset();
        hp.reset(); hfHp.reset(); aspBp.reset(); sat.reset(); dc.reset(); hfEnv.reset(); aspEnv.reset(); hyst.reset();
    }

    double process (double x, double gate)
    {
        if (cfg.nrType != 0 && cfg.nrEncode) x = enc.encode (x);
        x = pre.process (x);
        x = biasPre.process (x);
        if (cfg.satOn)
        {
            if (cfg.hysteresis) x = dc.process (hyst.process (x));
            else x = dc.process (sat.process (x * cfg.drive, cfg.satHard, cfg.satBias) / cfg.drive * adaaComp);
        }
        if (cfg.compOn && cfg.hfCompAmt > 0.001)
        {
            const double h = hfHp.process (x);
            const double envDb = gainToDb (hfEnv.process (h) + 1e-9);
            const double thr = -10.0 - 26.0 * cfg.hfCompAmt * cfg.hfSens;
            const double gDb = -std::min (cfg.compCap, std::max (0.0, envDb - thr) * 0.7);
            x += (dbToGain (gDb) - 1.0) * h;
        }
        // noise recorded on the tape: hiss and modulation (asperity) noise
        if (cfg.hissGain > 0.0) x += rng.next() * cfg.hissGain * 1.7 * gate;
        if (cfg.asperity > 0.0) x += aspBp.process (rng.next()) * aspEnv.process (x) * cfg.asperity * 0.25;
        x = de.process (x);
        if (cfg.respOn)
        {
            x = lp3.process (lp2.process (lp1.process (x)));
            x = bump.process (x);
            x = hp.process (x);
        }
        if (cfg.nrType != 0 && cfg.nrDecode) x = dec.decode (x, cfg.nrTrack);
        return x;
    }
};

// Recorder automatic level control (built-in compressor of cheap cassette recorders)
struct Alc
{
    Envelope env; double thr = -24, ratio = 3, makeup = 6;
    void configure (double fs, int mode)
    {
        if (mode == 1) { thr = -26; ratio = 4;  makeup = 9;  env.set (fs, 0.008, 0.45); }
        else           { thr = -42; ratio = 20; makeup = 22; env.set (fs, 0.001, 1.10); }
    }
    void reset() { env.reset(); }
    double process (double x, double amount)
    {
        const double L = gainToDb (env.process (x) + 1e-9);
        const double gr = L > thr ? (L - thr) * (1.0 - 1.0 / ratio) : 0.0;
        return x * dbToGain ((makeup - gr) * amount);
    }
};

// ----------------------------------------------------------------------------------------------
// Vinyl: cutting stage, surface noise and playback (stylus, cartridge, turntable)
struct VinylChan
{
    Biquad cutHp, traceHp, innerLp, cart1, cart2, tableLp, tableHp, tableMid, recLp1, recLp2, recHp,
           crackleBp, popLp, noiseLp, rumbleLp;
    Envelope cutEnv; AdaaSat sat; DcBlocker dc; Rng rng;
    Riaa riaaRec, riaaPlay; Tracer tracer; double gPrev = 0.0, yPrev = 0.0, velScale = 0.05, leak = 0.9999; bool tracing = true;
    double popEnv = 0, popSign = 1, popDecay = 0.99;
    double fs = 48000, traceAmt = 0, drive = 1, cutLimit = 0, noiseGain = 0, rumbleGain = 0;
    bool satOn = true;
    double crackleRate = 0, crackleAmp = 0, dustRate = 0, dustAmp = 0, popRate = 0, popAmp = 0;

    void reset()
    {
        for (auto* b : { &cutHp, &traceHp, &innerLp, &cart1, &cart2, &tableLp, &tableHp, &tableMid, &recLp1,
                         &recLp2, &recHp, &crackleBp, &popLp, &noiseLp, &rumbleLp })
            b->reset();
        cutEnv.reset(); sat.reset(); dc.reset(); popEnv = 0;
        riaaRec.reset(); riaaPlay.reset(); tracer.reset(); gPrev = yPrev = 0.0;
    }

    // surface events shared between channels are passed in (correlated part)
    double surface (double sharedClick, double sharedPop)
    {
        double s = 0.0;
        if (rng.uniform() < crackleRate / fs) s += crackleAmp * std::pow (rng.uniform(), 4.0) * (rng.next() > 0 ? 1 : -1);
        if (rng.uniform() < dustRate / fs)    s += dustAmp * std::pow (rng.uniform(), 3.0) * (rng.next() > 0 ? 1 : -1);
        s += sharedClick;
        double out = crackleBp.process (s) * 2.0 + s * 0.35;

        if (rng.uniform() < popRate / fs) { popEnv = popAmp * (0.3 + 0.7 * rng.uniform()); popSign = rng.next() > 0 ? 1 : -1; }
        if (sharedPop != 0.0) { popEnv = std::max (popEnv, std::abs (sharedPop)); popSign = sharedPop > 0 ? 1 : -1; }
        out += popLp.process (popEnv * (0.6 * popSign + 0.4 * rng.next()));
        popEnv *= popDecay;

        out += noiseLp.process (rng.next()) * noiseGain;
        out += rumbleLp.process (rng.next()) * rumbleGain;
        return out;
    }

    double process (double x, double surf)
    {
        // cutting: high-frequency limiter (lathe protection)
        if (cutLimit > 0.001)
        {
            const double h = cutHp.process (x);
            const double L = gainToDb (cutEnv.process (h) + 1e-9);
            const double thr = -10.0 - 22.0 * cutLimit;
            x += (dbToGain (-std::min (18.0, std::max (0.0, L - thr))) - 1.0) * h;
        }
        if (satOn) x = dc.process (sat.process (x * drive, false, 0.04) / drive);
        x += surf;
        // stylus tracing: audio -> RIAA cut -> groove displacement -> spherical tip -> velocity -> RIAA playback
        if (tracing)
        {
            const double vel = riaaRec.process (x) * velScale;                 // groove velocity (m/s)
            const double g = gPrev * leak + vel / fs;                          // displacement (m)
            gPrev = g;
            const double y = tracer.process (g);
            const double v2 = (y - yPrev) * fs;
            yPrev = y;
            x = riaaPlay.process (v2 / velScale);
        }
        if (traceAmt > 0.0) { const double h = traceHp.process (x); x += traceAmt * h * std::abs (h) * 4.0; }   // wear only
        x = innerLp.process (x);
        x = cart2.process (cart1.process (x));
        x = tableMid.process (tableHp.process (tableLp.process (x)));
        x = recHp.process (recLp2.process (recLp1.process (x)));
        return x;
    }
};

// ----------------------------------------------------------------------------------------------
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& b : transport) b.init ((int) (fs * 8.0));
        for (auto& b : print)     b.init ((int) (fs * 3.2));
        for (auto& b : echoBuf)   b.init ((int) (fs * 2.2));
        for (auto& b : dryBuf)    b.init ((int) (fs * 8.0));
        for (auto& b : align)     b.init (64);
        for (int c = 0; c < 2; ++c)
        {
            for (auto& p : cass[c]) p.rng.s = 0x9E3779B9u * (uint32_t) (c + 1) + 17u * (uint32_t) (&p - &cass[c][0] + 1);
            for (auto& p : vhs[c])  p.rng.s = 0x85EBCA6Bu * (uint32_t) (c + 1) + 31u * (uint32_t) (&p - &vhs[c][0] + 1);
            vin[c].rng.s = 0xC2B2AE35u + (uint32_t) c * 7919u;
            dmgRng[c].s = 0x27D4EB2Fu + (uint32_t) c * 104729u;
        }
        wowRnd.rng.s = 0x165667B1u; driftRnd.rng.s = 0xD3A2646Cu; degRnd[0].rng.s = 0xFD7046C5u; degRnd[1].rng.s = 0xB55A4F09u;
        shared.s = 0x6C8E9CF5u;
        reset();
        setParams (p);
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& x : cass[c]) x.reset();
            for (auto& x : vhs[c]) x.reset();
            vin[c].reset(); alc[c].reset();
            transport[c].clear(); print[c].clear(); echoBuf[c].clear(); dryBuf[c].clear(); align[c].clear();
            for (auto* b : { &dropLp[c], &degLp[c], &azLp[c], &glitchLp[c], &buzzHp[c], &motorBp[c], &printBp[c],
                             &echoHp[c], &echoLp[c], &eqHp[c], &eqLp[c], &eqLow[c], &eqHigh[c], &monoBassHp[c] })
                b->reset();
            dropEnv[c] = 0; dropTarget[c] = 0; dropLeft[c] = 0;
        }
        gateEnv.reset(); gate = 1; wowPh = flutPh = rotPh = humPh = buzzPh = bleedPh = 0;
        speed = 1; extra = 0; stopped = false; fadeBack = -1; snapT = -1; glitchEnv = 0; glitchLeft = 0;
        inGainSm = dbToGain (p.inGainDb);
    }

    void setParams (const Params& np)
    {
        const bool mediaChanged = np.media != p.media;
        const bool structural = np.hFormat != p.hFormat || np.hSpeed != p.hSpeed || np.cDeck != p.cDeck || np.cTape != p.cTape
                                || np.cSpeed != p.cSpeed || np.cNr != p.cNr || np.vRecord != p.vRecord || np.vTable != p.vTable
                                || np.vCart != p.vCart || np.hDolby != p.hDolby;
        p = np; p.fs = fs;
        if (mediaChanged)
            reset();   // start the newly selected medium from a clean state (no compander transients)
        else if (structural)
        {
            for (int c = 0; c < 2; ++c) { for (auto& x : cass[c]) x.reset(); for (auto& x : vhs[c]) x.reset(); vin[c].reset(); }
        }
        if (mediaChanged || structural) fadeIn = 0.0;   // short fade so that switching never clicks
        const double age = p.age;

        // ---------------- cassette passes
        {
            const auto& d = kDecks[std::clamp (p.cDeck, 0, 5)];
            const auto& t = kTapes[std::clamp (p.cTape, 0, 3)];
            const double spd = p.cSpeed == 1 ? 1.6 : 1.0;
            const double biasMul = p.cBias > 0 ? 1.0 - 0.3 * p.cBias : 1.0 + 0.15 * (-p.cBias);
            TapePassConfig c;
            c.nrType = p.cNr; c.nrEncode = p.cNrMode != 2; c.nrDecode = p.cNrMode != 1; c.nrTrack = p.cNrTrack;
            c.emphHz = t.emphHz; c.emphDb = 8.0;
            c.biasPreDb = 4.0 * std::max (0.0, -p.cBias);
            c.satOn = p.secSat && p.satAmt > 0.005; c.satHard = p.cSatModel == 2; c.satBias = t.asym;
            c.hysteresis = p.cSatModel == 0; c.hystBias = p.cBias;
            c.drive = t.drive * p.satAmt * dbToGain (p.drive * 40.0 - 8.0 + 4.0 * std::max (0.0, -p.cBias) - 2.0 * std::max (0.0, p.cBias));
            c.hystBias = std::clamp (p.cBias - 0.3 * std::max (0.0, p.satAmt - 1.0), -1.0, 1.0);   // pushing past 100 % also widens the loop
            c.compOn = p.secComp && p.compAmt > 0.005; c.hfCompAmt = p.cHfComp * p.compAmt; c.hfSens = t.hfSens;
            c.compCap = 14.0 * std::clamp (p.compAmt, 1.0, 2.5);
            if (std::abs (c.drive - cassHystDrive) > 1e-6 || std::abs (c.hystBias - cassHystBias) > 1e-6)
            {
                cassHyst.configure (c.drive, c.hystBias);
                cassHystDrive = c.drive; cassHystBias = c.hystBias;
            }
            c.hk = cassHyst.k; c.hc = cassHyst.c; c.hScale = cassHyst.hScale; c.hNorm = cassHyst.norm;
            const double hissDb = t.hissDb + d.hissDb + (p.cSpeed == 1 ? -3.0 : 0.0) + 1.5 * p.hissDb + 14.0 * age;
            c.hissGain = p.hissDb <= -47.9 ? 0.0 : dbToGain (hissDb);
            c.asperity = p.cAsperity * 2.5;
            c.respOn = p.secResp && p.respAmt > 0.005;
            {
                const double ra = p.respAmt;
                const double lpNom = std::min (20000.0, t.hfHz * d.hfMul * spd * biasMul * (1.0 - 0.75 * age) * std::pow (2.0, p.hfTrimOct));
                c.lpHz = morphHz (22000.0, lpNom, ra, 300.0, 22000.0);
                c.hpHz = morphHz (10.0, d.lowCutHz * std::pow (2.0, p.lowTrimOct), ra, 5.0, 2000.0);
                c.bumpHz = d.bumpHz * (p.cSpeed == 1 ? 2.0 : 1.0) * p.bumpMul;
                c.bumpDb = std::clamp ((d.bumpDb + p.cBumpDb) * ra, -18.0, 18.0);
                c.slope = p.hfSlope;
            }
            for (int ch = 0; ch < 2; ++ch)
                for (auto& pass : cass[ch]) pass.configure (fs, c);
            cassXtalk = dbToGain (d.xtalkDb + p.cXtalkOffset);
            cassWow = d.wowPct * 2.5; cassFlut = d.flutPct * 2.0; cassDrift = d.driftMul * 1.5;
            for (auto& a : alc) a.configure (fs, std::max (1, p.cAlc));
        }

        // ---------------- VHS passes
        {
            const auto& s = kVhsSpeeds[std::clamp (p.hSpeed, 0, 2)];
            const bool hifi = p.hFormat == 2;
            TapePassConfig c;
            c.nrType = hifi ? NoiseReduction::HiFi : (p.hDolby ? NoiseReduction::BType : NoiseReduction::Off);
            c.nrTrack = hifi ? p.hCompTrack : 0.0;
            c.emphHz = 2274; c.emphDb = hifi ? 0.0 : 6.0;
            c.satOn = p.secSat && p.satAmt > 0.005; c.satHard = false; c.satBias = 0.08;
            c.hysteresis = ! hifi; c.hystBias = 0.0;
            c.drive = (hifi ? 0.5 : 1.4) * p.satAmt * dbToGain (p.drive * 40.0 - 8.0);
            c.compOn = p.secComp && ! hifi && p.compAmt > 0.005; c.hfCompAmt = 0.5 * p.compAmt; c.hfSens = 1.1;
            c.compCap = 14.0 * std::clamp (p.compAmt, 1.0, 2.5);
            if (std::abs (c.drive - vhsHystDrive) > 1e-6)
            {
                vhsHyst.configure (c.drive, 0.0);
                vhsHystDrive = c.drive;
            }
            c.hk = vhsHyst.k; c.hc = vhsHyst.c; c.hScale = vhsHyst.hScale; c.hNorm = vhsHyst.norm;
            const double hissDb = (hifi ? -86.0 : s.hissDb) + 1.5 * p.hissDb + 14.0 * age;
            c.hissGain = p.hissDb <= -47.9 ? 0.0 : dbToGain (hissDb);
            c.asperity = hifi ? 0.0 : 0.2;
            c.respOn = p.secResp && p.respAmt > 0.005;
            {
                const double ra = p.respAmt;
                const double lpNom = (hifi ? 20000.0 : s.bwHigh * (1.0 - 0.4 * age)) * std::pow (2.0, p.hfTrimOct);
                c.lpHz = morphHz (22000.0, std::min (lpNom, 22000.0), ra, 300.0, 22000.0);
                c.hpHz = morphHz (10.0, (hifi ? 20.0 : s.bwLow) * std::pow (2.0, p.lowTrimOct), ra, 5.0, 2000.0);
                c.bumpHz = 100.0 * p.bumpMul; c.bumpDb = (hifi ? 0.0 : 1.0) * ra; c.slope = p.hfSlope;
            }
            for (int ch = 0; ch < 2; ++ch)
                for (auto& pass : vhs[ch]) pass.configure (fs, c);
            vhsWow  = hifi ? 0.02 : s.wowPct * 2.5;
            vhsFlut = hifi ? 0.015 : s.flutPct * 2.0;
            vhsXtalk = hifi ? 0.0 : dbToGain (-22.0);
            frameHz = p.hStandard == 1 ? 59.94 : 50.0;
            bleedHz = p.hStandard == 1 ? 15734.0 : 15625.0;
        }

        // ---------------- vinyl
        {
            const auto& r = kRecords[std::clamp (p.vRecord, 0, 3)];
            const auto& tb = kTables[std::clamp (p.vTable, 0, 3)];
            const double rpmFac = 33.333 / r.rpm;
            rotHz = r.rpm / 60.0;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& v = vin[ch];
                v.fs = fs;
                v.cutLimit = std::min (1.0, p.vCutLimit * p.compAmt);
                v.satOn = p.secSat && p.satAmt > 0.005;
                v.cutHp.highpass (fs, 6000.0, 0.7);
                v.cutEnv.set (fs, 0.001, 0.05);
                v.drive = r.drive * std::max (p.satAmt, 0.01) * dbToGain (p.drive * 14.0 - 7.0 + 4.0 * p.vWear);
                const double ra = p.secResp ? p.respAmt : 0.0;
                const double hfMul = std::pow (2.0, p.hfTrimOct), loMul = std::pow (2.0, p.lowTrimOct);
                v.traceAmt = 0.6 * p.vWear * rpmFac;
                {
                    static const double radius[3] = { 15e-6, 7e-6, 28e-6 };      // conical, elliptical, worn tip
                    const double rTip = radius[std::clamp (p.vStylus, 0, 2)] * (1.0 + 0.5 * p.vWear);
                    const double grooveR = 0.146 - p.vPos * (0.146 - 0.060);          // outer -> inner groove radius (m)
                    const double speed = 2 * kPi * grooveR * r.rpm / 60.0;
                    v.tracer.configure (fs, rTip, speed);
                    v.riaaRec.configure (fs, false); v.riaaPlay.configure (fs, true);
                    v.velScale = 0.07 * dbToGain (p.drive * 18.0 - 6.0) * std::max (p.satAmt, 0.05);   // cutting level
                    v.leak = 1.0 - 2 * kPi * 2.0 / fs;
                    v.tracing = p.secSat && p.satAmt > 0.005;
                }
                v.traceHp.highpass (fs, 3000.0, 0.7);
                auto lpF = [&] (double f) { return morphHz (22000.0, std::min (f * hfMul, 22000.0), ra, 300.0, 22000.0); };
                auto hpF = [&] (double f) { return morphHz (10.0, f * loMul, ra, 5.0, 2000.0); };
                v.innerLp.lowpass (fs, lpF (20000.0 * (1.0 - 0.5 * p.vPos * rpmFac) * (1.0 - 0.4 * p.vWear)), 0.7);
                if (p.vCart == 0)      { v.cart1.peak (fs, 20000.0, 0.8, 1.0 * ra); v.cart2.identity(); }
                else if (p.vCart == 1) { v.cart1.peak (fs, 12000.0, 1.4, 3.0 * ra); v.cart2.lowpass (fs, lpF (17000.0), 0.7); }
                else                   { v.cart1.peak (fs, 2500.0, 1.0, 3.0 * ra);  v.cart2.lowpass (fs, lpF (6000.0), 0.7); }
                v.tableLp.lowpass (fs, lpF (tb.lpHz), 0.7);
                v.tableHp.highpass (fs, hpF (tb.hpHz), 0.7);
                v.tableMid.peak (fs, 1300.0, 0.8, tb.midDb * ra);
                if (p.hfSlope == 0) { v.recLp1.lowpass (fs, lpF (r.bwHigh), 0.7071); v.recLp2.identity(); }
                else { v.recLp1.lowpass (fs, lpF (r.bwHigh), 0.54); v.recLp2.lowpass (fs, lpF (r.bwHigh * 1.1), 1.2); }
                v.recHp.highpass (fs, hpF (r.bwLow), 0.7);
                const double toneHz = 1500.0 + 6500.0 * p.vCrackleTone;
                v.crackleBp.bandpass (fs, toneHz, 1.2);
                v.popLp.lowpass (fs, 1800.0, 0.7);
                v.popDecay = std::exp (-1.0 / (0.003 * fs));
                v.noiseLp.lowpass (fs, 5000.0, 0.7);
                v.rumbleLp.lowpass (fs, 30.0, 0.7);
                v.crackleRate = 900.0 * std::pow (p.vCrackle + 0.4 * age, 1.3);
                v.crackleAmp  = 0.2 * (p.vCrackle + 0.4 * age);
                v.dustRate = 5000.0 * p.vDust;
                v.dustAmp  = 0.03 * p.vDust;
                v.popRate = 10.0 * p.vPops * p.vPops;
                v.popAmp  = 0.6 * p.vPops;
                const double nDb = r.noiseDb + 16.0 * p.vWear + 10.0 * p.vDust + 1.5 * p.hissDb + 14.0 * age;
                v.noiseGain = p.hissDb <= -47.9 ? 0.0 : dbToGain (nDb);
                v.rumbleGain = p.vRumbleDb <= -89.9 ? 0.0 : dbToGain (p.vRumbleDb + tb.rumbleDb) * 6.0;
            }
            vinXtalk = r.mono ? 1.0 : dbToGain (r.xtalkDb);
            vinMono = r.mono;
            vinWow = tb.wowPct * 2.5; vinFlut = tb.flutPct * 2.0;
            warpA = 0.03 * p.vWarp / (2 * kPi * rotHz);
            eccA  = 0.01 * p.vEcc / (2 * kPi * rotHz);
            static const double monoHz[4] = { 0.0, 80.0, 150.0, 300.0 };
            for (auto& b : monoBassHp) b.highpass (fs, std::max (20.0, monoHz[std::clamp (p.vMonoBass, 0, 3)]), 0.7);
            nScratch = p.vScratch <= 0.001 ? 0 : (p.vScratch < 0.4 ? 1 : (p.vScratch < 0.75 ? 2 : 3));
        }

        // ---------------- common
        for (int c = 0; c < 2; ++c)
        {
            dropLp[c].lowpass (fs, 700.0, 0.7);
            glitchLp[c].lowpass (fs, 3000.0, 0.7);
            azLp[c].lowpass (fs, 20000.0 * (1.0 - 0.85 * std::abs (p.cAzimuth)), 0.7);
            buzzHp[c].highpass (fs, 300.0, 0.7);
            motorBp[c].bandpass (fs, 80.0, 0.8);
            printBp[c].bandpass (fs, 1500.0, 0.6);
            echoHp[c].highpass (fs, p.echoHP, 0.7);
            echoLp[c].lowpass (fs, p.echoLP, 0.7);
            if (p.lowCut > 11.0) eqHp[c].highpass (fs, p.lowCut, 0.7); else eqHp[c].identity();
            if (p.highCut < 21000.0) eqLp[c].lowpass (fs, p.highCut, 0.7); else eqLp[c].identity();
            eqLow[c].lowShelf (fs, 300.0, -6.0 * p.tone);
            eqHigh[c].highShelf (fs, 3000.0, 6.0 * p.tone);
        }
        gateEnv.set (fs, 0.005, 0.3);
    }

    // Process one stereo sample. hostPlaying is used when "follow host transport" drives the stop.
    void process (float inL, float inR, float& outL, float& outR, bool stopRequested)
    {
        inGainSm += (dbToGain (p.inGainDb) - inGainSm) * 0.002;
        double x[2] = { inL * inGainSm, inR * inGainSm };
        double dry[2] = { inL, inR };
        dryBuf[0].write (inL); dryBuf[1].write (inR);

        // hiss gate (noise only while there is signal)
        {
            const double e = gateEnv.process (0.5 * (x[0] + x[1]));
            const double target = p.hissGate ? std::clamp ((gainToDb (e + 1e-9) + 70.0) / 20.0, 0.0, 1.0) : 1.0;
            gate += (target - gate) * 0.001;
        }

        // ================= media colour =================
        if (p.media == MediaCassette)
        {
            const int gens = std::clamp (p.cGen, 1, 4);
            for (int c = 0; c < 2; ++c)
            {
                if (p.cAlc > 0) x[c] = alc[c].process (x[c], p.cAlcAmt);
                for (int g = 0; g < gens; ++g) x[c] = cass[c][(size_t) g].process (x[c], gate);
            }
            crosstalk (x, cassXtalk);
        }
        else if (p.media == MediaVhs)
        {
            if (p.hFormat == 0) { const double m = 0.5 * (x[0] + x[1]); x[0] = x[1] = m; }
            const int gens = std::clamp (p.hGen, 1, 4);
            for (int c = 0; c < 2; ++c)
                for (int g = 0; g < gens; ++g) x[c] = vhs[c][(size_t) g].process (x[c], gate);
            if (p.hFormat == 1) crosstalk (x, vhsXtalk);
        }
        else // vinyl
        {
            if (vinMono) { const double m = 0.5 * (x[0] + x[1]); x[0] = x[1] = m; }
            else if (p.vMonoBass > 0)
            {
                const double m = 0.5 * (x[0] + x[1]);
                const double s = monoBassHp[0].process (0.5 * (x[0] - x[1]));
                x[0] = m + s; x[1] = m - s;
            }
            // correlated surface events (half of the crackle lands on both groove walls), periodic scratches
            double sharedClick = 0, sharedPop = 0;
            const auto& v0 = vin[0];
            if (shared.uniform() < 0.5 * v0.crackleRate / fs)
                sharedClick = v0.crackleAmp * std::pow (shared.uniform(), 4.0) * (shared.next() > 0 ? 1 : -1);
            const double prevRot = rotPh;
            rotPh += rotHz * std::max (speed, 0.0) / fs;
            if (rotPh >= 1.0) rotPh -= 1.0;
            for (int k = 0; k < nScratch; ++k)
            {
                const double pos = 0.13 + 0.29 * k;
                const bool crossed = (prevRot < pos && rotPh >= pos) || (prevRot > rotPh && (pos > prevRot || pos <= rotPh));
                if (crossed) sharedPop = 0.8 * p.vScratch * (k % 2 ? -1 : 1);
            }
            for (int c = 0; c < 2; ++c)
            {
                const double surf = vin[c].surface (sharedClick, sharedPop);
                x[c] = vin[c].process (x[c], surf);
            }
            crosstalk (x, vinXtalk);
        }

        // keep a constant latency: vinyl has the stylus look-ahead, the other media are delayed to match
        if (p.media != MediaVinyl || ! vin[0].tracing)
            for (int c = 0; c < 2; ++c) { align[c].write ((float) x[c]); x[c] = align[c].read ((double) Tracer::K); }

        // ================= transport =================
        transportStep (stopRequested);
        const double modS = modulationSeconds();
        const double baseD = 2.0 + modS * fs;
        double t[2];
        for (int c = 0; c < 2; ++c)
        {
            transport[c].write ((float) x[c]);
            double az = 0.0;
            if (p.media == MediaCassette && p.cAzimuth != 0.0)
                az = (c == 1 ? std::max (0.0, p.cAzimuth) : std::max (0.0, -p.cAzimuth)) * 0.0006 * fs;
            const double d = baseD + extra + az;
            double y = transport[c].read (d);
            if (fadeBack >= 0.0)
            {
                const double g = 0.5 - 0.5 * std::cos (kPi * fadeBack);
                y = (1.0 - g) * y + g * transport[c].read (baseD + az);
            }
            if (p.media == MediaCassette && p.cAzimuth != 0.0) y = azLp[c].process (y);
            t[c] = y;
            // dry path: the aligned mode reads the dry signal through the same transport (same delay,
            // same wow and flutter), so blending never produces a moving comb filter (phasing).
            if (p.mixMode == 1)
            {
                double dd = dryBuf[c].read (baseD + extra + Tracer::K);
                if (fadeBack >= 0.0)
                {
                    const double g = 0.5 - 0.5 * std::cos (kPi * fadeBack);
                    dd = (1.0 - g) * dd + g * dryBuf[c].read (baseD + Tracer::K);
                }
                dry[c] = dd;
            }
            else
                dry[c] = dryBuf[c].read (2.0 + Tracer::K);   // classic: only the fixed offset is compensated
        }
        if (fadeBack >= 0.0) { fadeBack += 1.0 / (0.05 * fs); if (fadeBack >= 1.0) { fadeBack = -1.0; extra = 0.0; } }
        const double stopGain = stopped ? 0.0 : std::clamp (speed / 0.04, 0.0, 1.0);
        x[0] = t[0] * stopGain; x[1] = t[1] * stopGain;
        if (p.mixMode == 1) { dry[0] *= stopGain; dry[1] *= stopGain; }

        // ================= damage =================
        damage (x);

        // ================= hum, motor, VHS artefacts =================
        {
            double add = 0.0;
            if (p.humDb > -89.9)
            {
                humPh += p.humHz / fs; if (humPh >= 1.0) humPh -= 1.0;
                const double w = 2 * kPi * humPh;
                add += dbToGain (p.humDb) * (std::sin (w) + 0.5 * std::sin (2 * w) + 0.3 * std::sin (3 * w));
            }
            double mot[2] = { 0, 0 };
            if (p.motorDb > -89.9)
                for (int c = 0; c < 2; ++c) mot[c] = motorBp[c].process (dmgRng[c].next()) * dbToGain (p.motorDb) * 3.0;
            if (p.media == MediaVhs)
            {
                buzzPh += frameHz / fs;   // head switching once per field (50 / 59.94 Hz)
                double spike = 0.0;
                if (buzzPh >= 1.0) { buzzPh -= 1.0; buzzCount = (int) (0.00015 * fs) + 1; }
                if (buzzCount > 0) { spike = 1.0; --buzzCount; }
                const double buzzLevel = p.hSwitch * (p.hFormat == 2 ? 0.05 : 0.015);
                if (bleedHz < fs * 0.45 && p.hBleedDb > -89.9)
                {
                    bleedPh += bleedHz / fs; if (bleedPh >= 1.0) bleedPh -= 1.0;
                    add += dbToGain (p.hBleedDb) * std::sin (2 * kPi * bleedPh);
                }
                for (int c = 0; c < 2; ++c) x[c] += buzzHp[c].process (spike) * buzzLevel;
            }
            x[0] += (add + mot[0]) * stopGain; x[1] += (add + mot[1]) * stopGain;
        }

        // ================= width / mono =================
        {
            const double m = 0.5 * (x[0] + x[1]);
            const double s = p.mono ? 0.0 : 0.5 * (x[0] - x[1]) * p.width;
            x[0] = m + s; x[1] = m - s;
        }

        // ================= print-through (cassette) =================
        if (p.media == MediaCassette && p.cPrintDb > -79.9)
        {
            const double g = dbToGain (p.cPrintDb);
            for (int c = 0; c < 2; ++c)
            {
                print[c].write ((float) x[c]);
                x[c] += printBp[c].process (print[c].read (p.cPrintTime * fs)) * g;
            }
        }

        // ================= tape echo =================
        if (p.echoType > 0)
        {
            const double d = std::max (2.0, p.echoMs * 0.001 * fs + modS * fs * 0.5);
            const double e0 = echoBuf[0].read (d), e1 = echoBuf[1].read (d);
            auto shape = [this] (int c, double v) { return std::tanh (echoLp[c].process (echoHp[c].process (v)) * 1.2) / 1.2; };
            if (p.echoType == 1)
            {
                echoBuf[0].write ((float) x[0]); echoBuf[1].write ((float) x[1]);
                x[0] += p.echoLevel * shape (0, e0); x[1] += p.echoLevel * shape (1, e1);
            }
            else if (p.echoType == 2)
            {
                const double f0 = shape (0, e0), f1 = shape (1, e1);
                echoBuf[0].write ((float) (x[0] + p.echoFb * f0)); echoBuf[1].write ((float) (x[1] + p.echoFb * f1));
                x[0] += p.echoLevel * f0; x[1] += p.echoLevel * f1;
            }
            else
            {
                const double f0 = shape (0, e0), f1 = shape (1, e1);
                echoBuf[0].write ((float) (0.5 * (x[0] + x[1]) + p.echoFb * f1));
                echoBuf[1].write ((float) (p.echoFb * f0));
                x[0] += p.echoLevel * f0; x[1] += p.echoLevel * f1;
            }
        }

        // ================= tone EQ, gains, mix =================
        double comp = (p.autoGain ? 1.0 / inGainSm : 1.0) * dbToGain (p.outGainDb);
        if (fadeIn < 1.0) { comp *= fadeIn * fadeIn; fadeIn = std::min (1.0, fadeIn + 1.0 / (0.02 * fs)); }
        double o[2];
        for (int c = 0; c < 2; ++c)
        {
            double y = eqHigh[c].process (eqLow[c].process (eqLp[c].process (eqHp[c].process (x[c]))));
            y *= comp;
            y = 4.0 * std::tanh (y * 0.25);   // safety: transparent at normal levels, tames runaway settings
            o[c] = dry[c] * (1.0 - p.mix) + y * p.mix;
            if (! std::isfinite (o[c])) o[c] = 0.0;
        }
        outL = (float) o[0]; outR = (float) o[1];
    }

    double currentSpeed() const { return stopped ? 0.0 : speed; }

private:
    static void crosstalk (double* x, double k)
    {
        if (k <= 0.0) return;
        const double l = x[0], r = x[1];
        x[0] = (l + k * r) / (1.0 + k);
        x[1] = (r + k * l) / (1.0 + k);
    }

    void transportStep (bool stopRequested)
    {
        if (stopRequested)
        {
            if (! stopped)
            {
                speed -= 1.0 / (std::max (p.stopTime, 0.01) * fs);
                if (speed <= 0.0) { speed = 0.0; stopped = true; extra = 0.0; fadeBack = -1.0; }
            }
        }
        else
        {
            if (stopped) { stopped = false; speed = 0.0; extra = 0.0; }
            if (speed < 1.0)
            {
                speed += 1.0 / (std::max (p.startTime, 0.01) * fs);
                if (speed >= 1.0) { speed = 1.0; if (extra > 1.0) fadeBack = 0.0; else extra = 0.0; }
            }
        }
        if (! stopped && speed < 1.0)
            extra = std::min (extra + (1.0 - speed), (double) (transport[0].capacity() - fs * 0.2));
    }

    double modulationSeconds()
    {
        double wowPct, flutPct, driftMul;
        if (p.media == MediaCassette) { wowPct = cassWow; flutPct = cassFlut; driftMul = cassDrift; }
        else if (p.media == MediaVhs) { wowPct = vhsWow;  flutPct = vhsFlut;  driftMul = p.hFormat == 2 ? 0.2 : 0.8; }
        else                          { wowPct = vinWow;  flutPct = vinFlut;  driftMul = 0.2; }

        const double amt = p.wfAmount * (1.0 + 2.0 * p.age);
        const double wowHz = p.wowSyncHz > 0.0 ? p.wowSyncHz : std::max (0.05, p.wowRate);
        const double flutHz = std::max (1.0, p.flutRate);

        // wow: sine blended with a smoothed random component
        wowPh += wowHz / fs; if (wowPh >= 1.0) wowPh -= 1.0;
        const double wr = wowRnd.next (wowHz * 1.3, fs);
        const double wowWave = (1.0 - p.wfRandom) * std::sin (2 * kPi * wowPh) + p.wfRandom * std::clamp (wr * 1.4, -1.0, 1.0);
        const double wowA = wowPct * 0.01 * p.wowDepth * amt / (2 * kPi * wowHz);

        // flutter (optionally frequency-modulated by the wow)
        flutPh += flutHz * (1.0 + 0.8 * p.fm * wowWave) / fs; if (flutPh >= 1.0) flutPh -= 1.0;
        const double flutA = flutPct * 0.01 * p.flutDepth * amt / (2 * kPi * flutHz);
        const double flutWave = std::sin (2 * kPi * flutPh) * (1.0 - 0.5 * p.wfRandom) + 0.5 * p.wfRandom * wowRnd.rng.next() * 0.3;

        // slow drift
        const double driftA = p.drift * driftMul * 0.009 * p.wfAmount;
        const double driftWave = driftRnd.next (0.07, fs);

        // sticky tape snaps (pitch drop and recovery)
        double snapS = 0.0;
        if (snapT < 0.0 && p.snap > 0.0 && shared.uniform() < p.snap * 0.5 / fs) snapT = 0.0;
        if (snapT >= 0.0)
        {
            snapS = 0.004 * p.snap * (0.5 - 0.5 * std::cos (2 * kPi * snapT));
            snapT += 1.0 / (0.12 * fs);
            if (snapT >= 1.0) snapT = -1.0;
        }

        double vinylS = 0.0;
        if (p.media == MediaVinyl)
        {
            const double ph = 2 * kPi * rotPh;
            vinylS = warpA * (1.0 + 0.77 * (std::sin (ph) + 0.3 * std::sin (2 * ph + 0.5))) + eccA * (1.0 + std::sin (ph + 1.1));
        }

        return wowA * (1.0 + wowWave) + flutA * (1.0 + flutWave) + driftA * (1.0 + driftWave) + snapS + vinylS;
    }

    void damage (double* x)
    {
        // VHS tracking errors (shared): Hi-Fi drops to a dull, buzzy sound; linear dips in level
        if (p.media == MediaVhs && p.hTracking > 0.0)
        {
            if (glitchLeft <= 0 && shared.uniform() < p.hTracking * p.hTracking * 6.0 / fs)
                glitchLeft = (int) ((0.04 + 0.4 * shared.uniform()) * fs);
            const double target = glitchLeft > 0 ? 1.0 : 0.0;
            if (glitchLeft > 0) --glitchLeft;
            glitchEnv += (target - glitchEnv) * (target > glitchEnv ? 0.02 : 0.003);
            if (glitchEnv > 1e-4)
                for (int c = 0; c < 2; ++c)
                {
                    if (p.hFormat == 2)
                        x[c] = (1.0 - glitchEnv) * x[c] + glitchEnv * (0.5 * glitchLp[c].process (x[c]) + 0.12 * p.hTracking * dmgRng[c].next());
                    else
                        x[c] *= 1.0 - 0.85 * glitchEnv;
                }
        }

        // dropouts (shared or per channel according to the stereo width)
        if (p.dropouts > 0.0)
        {
            const double rate = p.dropRate * (0.5 + 2.0 * p.age);
            bool sharedEvent = shared.uniform() < rate / fs * (1.0 - p.dropWidth);
            for (int c = 0; c < 2; ++c)
            {
                const bool own = dmgRng[c].uniform() < rate / fs * p.dropWidth;
                if ((sharedEvent || own) && dropLeft[c] <= 0)
                {
                    dropLeft[c] = (int) ((0.02 + 0.13 * dmgRng[c].uniform()) * fs);
                    dropTarget[c] = 0.5 + 0.5 * dmgRng[c].uniform();
                }
                const double target = dropLeft[c] > 0 ? dropTarget[c] : 0.0;
                if (dropLeft[c] > 0) --dropLeft[c];
                dropEnv[c] += (target - dropEnv[c]) * 0.004;
                const double e = dropEnv[c] * p.dropouts;
                if (e > 1e-4)
                    x[c] = ((1.0 - e) * x[c] + e * dropLp[c].process (x[c])) * (1.0 - 0.97 * e);
                else
                    dropLp[c].process (x[c]);
            }
        }

        // slow high-frequency degradation (wear)
        if (p.degrade > 0.0)
            for (int c = 0; c < 2; ++c)
            {
                const double m = p.degrade * (0.5 + 0.5 * degRnd[c].next (p.degradeRate, fs));
                degLp[c].lowpass (fs, 18000.0 * std::pow (0.05, std::clamp (m, 0.0, 1.0)), 0.7);
                x[c] = degLp[c].process (x[c]);
            }
    }

    double fs = 48000.0;
    Params p;

    std::array<std::array<TapePass, 4>, 2> cass, vhs;
    std::array<VinylChan, 2> vin;
    std::array<Alc, 2> alc;
    std::array<DelayBuf, 2> transport, print, echoBuf, dryBuf, align;
    std::array<Biquad, 2> dropLp, degLp, azLp, glitchLp, buzzHp, motorBp, printBp, echoHp, echoLp, eqHp, eqLp,
                          eqLow, eqHigh, monoBassHp;
    std::array<RandomLfo, 2> degRnd;
    std::array<Rng, 2> dmgRng;
    std::array<double, 2> dropEnv {}, dropTarget {};
    std::array<int, 2> dropLeft {};
    RandomLfo wowRnd, driftRnd;
    Rng shared;
    Envelope gateEnv;

    double gate = 1, wowPh = 0, flutPh = 0, rotPh = 0, humPh = 0, buzzPh = 0, bleedPh = 0;
    int buzzCount = 0, glitchLeft = 0, nScratch = 0;
    double glitchEnv = 0, speed = 1, extra = 0, fadeBack = -1, snapT = -1, inGainSm = 1, fadeIn = 1;
    bool stopped = false;

    double cassXtalk = 0, cassWow = 0.1, cassFlut = 0.08, cassDrift = 0.6;
    double vhsWow = 0.1, vhsFlut = 0.1, vhsXtalk = 0, frameHz = 50, bleedHz = 15625;
    double vinXtalk = 0, vinWow = 0.02, vinFlut = 0.015, rotHz = 0.5555, warpA = 0, eccA = 0;
    bool vinMono = false;
    Hysteresis cassHyst, vhsHyst;
    double cassHystDrive = -1.0, cassHystBias = 9.0, vhsHystDrive = -1.0;
};
} // namespace retro2
