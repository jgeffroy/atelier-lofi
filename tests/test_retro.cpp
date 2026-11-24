#include "RetroEngine.h"
#include <cstdio>
#include <vector>

using namespace retro;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++fails; } else std::printf("ok:   %s\n", msg); } while (0)

static const double FS = 48000.0;

static Params clean (int media)
{
    Params p; p.media = media;
    p.hissDb = -48; p.wfAmount = 0; p.drift = 0; p.cAsperity = 0; p.drive = 0.0; p.cHfComp = 0;
    p.vCrackle = p.vPops = p.vDust = p.vScratch = p.vWarp = p.vEcc = 0; p.vWear = 0; p.vPos = 0; p.vRumbleDb = -90;
    p.hTracking = 0; p.hSwitch = 0; p.hBleedDb = -90;
    return p;
}

struct Out { std::vector<double> L, R; };

static Out run (const Params& p, int n, double freq, double amp, bool stop = false, int stopAt = -1, int startAt = -1)
{
    Engine e; e.prepare (FS); e.setParams (p); e.reset();
    Out o;
    for (int i = 0; i < n; ++i)
    {
        const float in = freq > 0 ? (float) (amp * std::sin (2 * kPi * freq * i / FS)) : (i == 0 ? (float) amp : 0.0f);
        bool st = stop;
        if (stopAt >= 0) st = i >= stopAt && (startAt < 0 || i < startAt);
        float l, r; e.process (in, in, l, r, st);
        o.L.push_back (l); o.R.push_back (r);
    }
    return o;
}

static double goertzel (const std::vector<double>& x, size_t from, double f)
{
    const size_t per = (size_t) std::lround (FS / f);
    const size_t n = ((x.size() - from) / per) * per;
    double w = 2 * kPi * f / FS, c = 2 * std::cos (w), s1 = 0, s2 = 0;
    for (size_t i = from; i < from + n; ++i) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (n * 0.5);
}

static double rms (const std::vector<double>& x, size_t from)
{
    double s = 0; for (size_t i = from; i < x.size(); ++i) s += x[i] * x[i];
    return std::sqrt (s / (double) (x.size() - from));
}

static double respDb (const Params& p, double f) { auto o = run (p, 48000, f, 0.05); return 20 * std::log10 (goertzel (o.L, 12000, f) / 0.05); }

int main()
{
    // 1. Tape types: metal is brighter than ferric; dictaphone is much darker than a studio deck
    {
        Params a = clean (MediaCassette); a.cTape = 0;
        Params b = clean (MediaCassette); b.cTape = 3;
        const double fa = respDb (a, 10000), fb = respDb (b, 10000);
        std::printf ("      10 kHz: type I %.1f dB, type IV %.1f dB\n", fa, fb);
        CHECK (fb > fa + 3.0, "cassette: type IV brighter than type I");
        Params s = clean (MediaCassette); s.cDeck = 0;
        Params d = clean (MediaCassette); d.cDeck = 5;
        const double fs1 = respDb (s, 7000), fd = respDb (d, 7000);
        std::printf ("      7 kHz: studio %.1f dB, dictaphone %.1f dB\n", fs1, fd);
        CHECK (fd < fs1 - 12.0, "cassette: dictaphone far darker than studio deck");
        const double mid = respDb (b, 1000);
        CHECK (std::abs (mid) < 2.5, "cassette: about unity gain at 1 kHz");
    }

    // 2. Saturation increases with input level (auto gain on)
    {
        double h3[2];
        for (int k = 0; k < 2; ++k)
        {
            Params p = clean (MediaCassette); p.cDeck = 0; p.cTape = 0; p.inGainDb = k ? 18.0 : -6.0; p.drive = 0.5;
            auto o = run (p, 48000, 500, 0.3);
            h3[k] = goertzel (o.L, 12000, 1500) / goertzel (o.L, 12000, 500);
        }
        std::printf ("      H3/H1: input -6 dB %.4f, +18 dB %.4f\n", h3[0], h3[1]);
        CHECK (h3[1] > h3[0] * 4.0, "cassette: more input gives more saturation");
    }

    // 3. Noise reduction: B-type encode+decode about transparent, encode-only brighter, hiss reduced
    {
        Params off = clean (MediaCassette); off.cDeck = 0; off.cTape = 3;
        Params nb = off; nb.cNr = 1;
        Params enc = nb; enc.cNrMode = 1;
        const double f = 6000, a = 0.02;
        auto g = [&] (const Params& p) { auto o = run (p, 48000, f, a); return 20 * std::log10 (goertzel (o.L, 12000, f) / a); };
        const double gOff = g (off), gB = g (nb), gEnc = g (enc);
        std::printf ("      6 kHz at -34 dBFS: no NR %.1f, B %.1f, encode only %.1f dB\n", gOff, gB, gEnc);
        CHECK (std::abs (gB - gOff) < 2.0, "NR B: encode + decode about transparent");
        CHECK (gEnc > gOff + 4.0, "NR B: encode only brightens quiet highs");

        Params h0 = off; h0.hissDb = 0;
        Params h1 = h0; h1.cNr = 1;
        auto hfHiss = [&] (const Params& p) { auto o = run (p, 48000, 0, 0); Biquad hp; hp.highpass (FS, 4000.0); hp.process (0); for (auto& v : o.L) v = hp.process (v); return rms (o.L, 12000); };
        const double n0 = hfHiss (h0), n1 = hfHiss (h1);
        std::printf ("      hiss above 4 kHz: no NR %.2f dBFS, B %.2f dBFS\n", gainToDb (n0), gainToDb (n1));
        CHECK (n0 > 1e-6 && gainToDb (n1) < gainToDb (n0) - 6.0, "NR B lowers the high-frequency hiss");
        Params hOff = h0; hOff.hissDb = -48;
        CHECK (rms (run (hOff, 48000, 0, 0).L, 12000) < 1e-6, "hiss off gives silence");
    }

    // 4. Wow and flutter modulate the pitch
    {
        auto spread = [&] (double amount)
        {
            Params p = clean (MediaCassette); p.cDeck = 3; p.wfAmount = amount; p.wowDepth = 2;
            auto o = run (p, 96000, 1000, 0.3);
            std::vector<double> periods; double last = -1;
            for (size_t i = 20000; i + 1 < o.L.size(); ++i)
                if (o.L[i] <= 0 && o.L[i + 1] > 0)
                {
                    const double t = i + o.L[i] / (o.L[i] - o.L[i + 1]);
                    if (last > 0) periods.push_back (t - last);
                    last = t;
                }
            double m = 0; for (double v : periods) m += v; m /= periods.size();
            double s = 0; for (double v : periods) s += (v - m) * (v - m);
            return std::sqrt (s / periods.size()) / m;
        };
        const double s0 = spread (0.0), s1 = spread (2.0);
        std::printf ("      period deviation: stable %.5f, walkman x2 %.5f\n", s0, s1);
        CHECK (s1 > s0 * 5.0 && s1 > 0.001, "wow/flutter modulates the pitch");
    }

    // 5. Tape stop and restart
    {
        Params p = clean (MediaCassette); p.stopTime = 0.5; p.startTime = 0.3;
        auto o = run (p, 48000 * 3, 300, 0.3, false, 24000, 24000 + 48000);
        double during = 0, after = 0;
        for (int i = 24000 + 30000; i < 24000 + 46000; ++i) during = std::max (during, std::abs (o.L[(size_t) i]));
        for (int i = 24000 + 48000 + 30000; i < 48000 * 3; ++i) after = std::max (after, std::abs (o.L[(size_t) i]));
        std::printf ("      tape stop: peak while stopped %.5f, after restart %.3f\n", during, after);
        CHECK (during < 1e-3 && after > 0.2, "tape stop silences, start brings the signal back");
    }

    // 6. Vinyl: crackle on silence, periodic scratch at the rotation period
    {
        Params p = clean (MediaVinyl); p.vCrackle = 0.8;
        auto o = run (p, 48000, 0, 0);
        double pk = 0; for (double v : o.L) pk = std::max (pk, std::abs (v));
        std::printf ("      crackle peak %.4f\n", pk);
        CHECK (pk > 0.005, "vinyl: crackle on silence");

        Params s = clean (MediaVinyl); s.vScratch = 0.3;
        auto os = run (s, 48000 * 5, 0, 0);
        std::vector<int> hits; int lastHit = -100000;
        for (int i = 0; i < (int) os.L.size(); ++i)
            if (std::abs (os.L[(size_t) i]) > 0.02 && i - lastHit > 4800) { hits.push_back (i); lastHit = i; }
        double period = hits.size() > 1 ? (double) (hits.back() - hits.front()) / (hits.size() - 1) / FS : 0;
        std::printf ("      scratch hits %zu, period %.3f s (33 rpm = 1.800 s)\n", hits.size(), period);
        CHECK (hits.size() >= 2 && std::abs (period - 1.8) < 0.02, "vinyl: scratch repeats every revolution");

        Params o78 = clean (MediaVinyl); o78.vRecord = 2;
        CHECK (respDb (o78, 8000) < respDb (clean (MediaVinyl), 8000) - 12.0, "vinyl: shellac 78 is band-limited");
    }

    // 7. VHS: linear EP is far narrower than Hi-Fi
    {
        Params hf = clean (MediaVhs); hf.hFormat = 2;
        Params ep = clean (MediaVhs); ep.hFormat = 1; ep.hSpeed = 2;
        const double a = respDb (hf, 8000), b = respDb (ep, 8000);
        Params hfNoNr = hf; hfNoNr.secSat = false;
        std::printf ("      Hi-Fi: 1 kHz %.2f dB, 8 kHz no sat %.2f dB\n", respDb (hf, 1000), respDb (hfNoNr, 8000));
        std::printf ("      8 kHz: Hi-Fi %.1f dB, linear EP %.1f dB\n", a, b);
        CHECK (std::abs (a) < 3.0 && b < a - 15.0, "VHS: linear EP band-limited, Hi-Fi full range");
        Params bz = clean (MediaVhs); bz.hSwitch = 1.0;
        auto ob = run (bz, 48000, 0, 0);
        CHECK (goertzel (ob.L, 4800, 1000) > 1e-5 || rms (ob.L, 4800) > 1e-4, "VHS: head switching buzz on silence");
    }

    // 8. Print-through and echo land at the right time
    {
        Params p = clean (MediaCassette); p.cPrintDb = -30; p.cPrintTime = 1.0; p.secResp = false; p.secSat = false;
        auto o = run (p, 60000, 0, 0.8);
        int pk = 30000; double m = 0;
        for (int i = 30000; i < 60000; ++i) if (std::abs (o.L[(size_t) i]) > m) { m = std::abs (o.L[(size_t) i]); pk = i; }
        std::printf ("      print-through at %d samples (expect ~48000)\n", pk);
        CHECK (std::abs (pk - 48000) < 200, "print-through echo one revolution later");

        Params e = clean (MediaCassette); e.echoType = 1; e.echoMs = 250; e.echoLevel = 0.5; e.secResp = false; e.secSat = false;
        auto oe = run (e, 24000, 0, 0.8);
        pk = 5000; m = 0;
        for (int i = 5000; i < 24000; ++i) if (std::abs (oe.L[(size_t) i]) > m) { m = std::abs (oe.L[(size_t) i]); pk = i; }
        std::printf ("      echo at %d samples (expect ~12000)\n", pk);
        CHECK (std::abs (pk - 12000) < 40, "tape echo at the set time");
    }

    // 9. Dub generations lose more highs
    {
        Params g1 = clean (MediaCassette); g1.cTape = 0;
        Params g4 = g1; g4.cGen = 4;
        const double a = respDb (g1, 8000), b = respDb (g4, 8000);
        std::printf ("      8 kHz: 1 generation %.1f dB, 4 generations %.1f dB\n", a, b);
        CHECK (b < a - 6.0, "generations: more high-frequency loss");
    }

    // 10. Random settings stay finite and bounded
    {
        Rng r; r.s = 12345; bool ok = true; double worst = 0;
        for (int k = 0; k < 120; ++k)
        {
            Params p;
            p.media = (int) (r.uniform() * 3); p.inGainDb = -24 + 48 * r.uniform(); p.drive = r.uniform();
            p.cDeck = (int) (r.uniform() * 6); p.cTape = (int) (r.uniform() * 4); p.cSpeed = r.uniform() > 0.5;
            p.cBias = r.next(); p.cSatModel = r.uniform() > 0.5; p.cAlc = (int) (r.uniform() * 3); p.cNr = (int) (r.uniform() * 4);
            p.cNrMode = (int) (r.uniform() * 3); p.cNrTrack = 10 * r.next(); p.cGen = 1 + (int) (r.uniform() * 4);
            p.cAzimuth = r.next(); p.cPrintDb = -80 + 60 * r.uniform(); p.vTable = (int) (r.uniform() * 4);
            p.vCart = (int) (r.uniform() * 3); p.vRecord = (int) (r.uniform() * 4); p.vPos = r.uniform(); p.vWear = r.uniform();
            p.vCrackle = r.uniform(); p.vPops = r.uniform(); p.vDust = r.uniform(); p.vScratch = r.uniform(); p.vWarp = r.uniform();
            p.vMonoBass = (int) (r.uniform() * 4); p.vCutLimit = r.uniform(); p.hFormat = (int) (r.uniform() * 3);
            p.hSpeed = (int) (r.uniform() * 3); p.hTracking = r.uniform(); p.hSwitch = r.uniform(); p.hCompTrack = 10 * r.next();
            p.hGen = 1 + (int) (r.uniform() * 4); p.wfAmount = 3 * r.uniform(); p.wowDepth = 4 * r.uniform(); p.flutDepth = 4 * r.uniform();
            p.wowRate = 0.1 + 5 * r.uniform(); p.flutRate = 4 + 20 * r.uniform(); p.snap = r.uniform(); p.fm = r.uniform();
            p.hissDb = -48 + 72 * r.uniform(); p.humDb = -90 + 60 * r.uniform(); p.motorDb = -90 + 60 * r.uniform(); p.age = r.uniform();
            p.dropouts = r.uniform(); p.dropRate = 0.1 + 9.9 * r.uniform(); p.degrade = r.uniform();
            p.echoType = (int) (r.uniform() * 4); p.echoFb = r.uniform(); p.echoMs = 10 + 1990 * r.uniform();
            p.width = 2 * r.uniform(); p.tone = r.next();
            auto o = run (p, 12000, 220 + 2000 * r.uniform(), 0.9, r.uniform() > 0.8);
            for (size_t i = 0; i < o.L.size(); ++i)
            {
                if (! std::isfinite (o.L[i]) || ! std::isfinite (o.R[i])) ok = false;
                worst = std::max (worst, std::max (std::abs (o.L[i]), std::abs (o.R[i])));
            }
        }
        std::printf ("      worst peak over 120 random settings: %.2f\n", worst);
        CHECK (ok && worst < 12.0, "random settings: finite and bounded output");
    }

        // 11. Stage amounts and ranges
    {
        Params base = clean (MediaCassette); base.cTape = 0;
        Params r0 = base; r0.respAmt = 0;
        Params r4 = base; r4.respAmt = 4;
        const double a0 = respDb (r0, 9000), a1 = respDb (base, 9000), a4 = respDb (r4, 9000);
        std::printf ("      9 kHz: response 0%% %.1f dB, 100%% %.1f dB, x4 %.1f dB\n", a0, a1, a4);
        CHECK (std::abs (a0) < 1.5 && a4 < a1 - 6.0, "response amount: 0 is flat, x4 goes much further");
        double h[2];
        for (int k = 0; k < 2; ++k)
        {
            Params p = base; p.satAmt = k ? 8.0 : 1.0;
            auto o = run (p, 48000, 500, 0.2);
            h[k] = goertzel (o.L, 12000, 1500) / goertzel (o.L, 12000, 500);
        }
        std::printf ("      H3/H1: saturation x1 %.4f, x8 %.4f\n", h[0], h[1]);
        CHECK (h[1] > h[0] * 5.0, "saturation range x8 distorts much more");
        Params s12 = base; s12.hfSlope = 0; Params s36 = base; s36.hfSlope = 2;
        CHECK (respDb (s36, 16000) < respDb (s12, 16000) - 6.0, "HF slope 36 dB/oct steeper than 12 dB/oct");
    }

    // 12. Parallel mix: the aligned mode removes the moving comb filter (phasing) of the classic mode
    {
        auto wobble = [&] (int mode)
        {
            Params p = clean (MediaCassette); p.cDeck = 3; p.wfAmount = 2.0; p.mix = 0.5; p.mixMode = mode;
            p.respAmt = 0; p.satAmt = 0; p.compAmt = 0;
            auto o = run (p, 96000, 5000, 0.3);
            double mn = 1e9, mx = 0;
            for (size_t w = 19200; w + 2400 <= o.L.size(); w += 2400)
            {
                double e = 0; for (size_t i = w; i < w + 2400; ++i) e += o.L[i] * o.L[i];
                mn = std::min (mn, e); mx = std::max (mx, e);
            }
            return 10.0 * std::log10 (mx / mn);
        };
        const double classic = wobble (2), aligned = wobble (1);
        std::printf ("      level swing at 50%% mix, 5 kHz: classic %.1f dB, aligned %.2f dB\n", classic, aligned);
        CHECK (classic > 3.0 && aligned < 0.5, "aligned parallel mix: no phasing");
    }

    std::printf ("%s\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return fails;
}
