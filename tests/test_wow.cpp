#include "WowLab/WowEngine.h"
#include <cstdio>
#include <vector>
using namespace wow;
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++fails; } else std::printf("ok:   %s\n", m); } while (0)
static const double FS = 48000.0;
struct Out { std::vector<double> L, R; };
static Out run (const Params& p, double f, double a, int n, bool impulse = false)
{
    Engine e; e.prepare (FS); e.setParams (p); e.reset(); e.setTransport (false, 0, 120, -1);
    Out o;
    for (int i = 0; i < n; ++i)
    {
        double v = impulse ? (i == 100 ? a : 0.0) : a * std::sin (2 * retro::kPi * f * i / FS);
        double l = v, r = v; e.process (l, r); o.L.push_back (l); o.R.push_back (r);
    }
    return o;
}
static double gz (const std::vector<double>& x, size_t from, double f)
{
    double w = 2 * retro::kPi * f / FS, c = 2 * std::cos (w), s1 = 0, s2 = 0; size_t n = x.size() - from;
    for (size_t i = from; i < x.size(); ++i) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (n * 0.5);
}
static double periodSpread (const std::vector<double>& x)
{
    std::vector<double> per; double last = -1;
    for (size_t i = 4800; i + 1 < x.size(); ++i)
        if (x[i] <= 0 && x[i + 1] > 0) { double t = i + x[i] / (x[i] - x[i + 1]); if (last > 0) per.push_back (t - last); last = t; }
    double m = 0; for (double v : per) m += v; m /= per.size();
    double s = 0; for (double v : per) s += (v - m) * (v - m);
    return std::sqrt (s / per.size()) / m;
}
int main()
{
    // 1. no modulation, analog off: pure delay of the reported latency
    for (int hq = 0; hq < 2; ++hq)
    {
        Params p; p.amount = 0; p.analogOn = false; p.hq = hq;
        auto o = run (p, 0, 1.0, 400, true);
        int pk = 0; double m = 0; for (int i = 0; i < 400; ++i) if (std::abs (o.L[(size_t) i]) > m) { m = std::abs (o.L[(size_t) i]); pk = i; }
        const int lat = hq ? 17 : 2;
        char msg[96]; std::snprintf (msg, sizeof msg, "%s: transparent delay of %d samples (peak at %d, %.3f)", hq ? "HQ" : "normal", lat, pk - 100, m);
        CHECK (pk - 100 == lat && m > 0.9, msg);
    }
    // 2. amount and multiplier increase the pitch modulation
    {
        Params a; a.analogOn = false; a.amount = 0.15;
        Params b = a; b.amountMult = 16;
        const double sa = periodSpread (run (a, 1000, 0.5, 96000).L), sb = periodSpread (run (b, 1000, 0.5, 96000).L);
        std::printf ("      period deviation: 15%% x1 %.5f, 15%% x16 %.5f\n", sa, sb);
        CHECK (sb > sa * 8.0, "amount multiplier scales the modulation");
    }
    // 3. HQ interpolation: steadier level on high frequencies under modulation
    {
        Params p; p.analogOn = false; p.amount = 0.5; p.amountMult = 8; p.speedHz = 0.5; p.flutterBal = 0.0;
        auto swing = [&] (bool hq)
        {
            Params q = p; q.hq = hq; auto o = run (q, 15000, 0.5, 96000);
            double mn = 1e9, mx = 0;
            for (size_t w = 9600; w + 480 <= o.L.size(); w += 480)
            { double e = 0; for (size_t i = w; i < w + 480; ++i) e += o.L[i] * o.L[i]; mn = std::min (mn, e); mx = std::max (mx, e); }
            return 10 * std::log10 (mx / mn);
        };
        const double n = swing (false), h = swing (true);
        std::printf ("      15 kHz level swing under modulation: normal %.2f dB, HQ %.2f dB\n", n, h);
        CHECK (h < n * 0.5 && h < 0.5, "HQ mode keeps high frequencies steady (better interpolation)");
    }
    // 4. stereo phase 180 degrees: left and right modulate in opposition
    {
        Params p; p.analogOn = false; p.amount = 1.0; p.amountMult = 8; p.stereoPhaseDeg = 180; p.flutterBal = 0;
        auto o = run (p, 200, 0.5, 96000);
        double diff = 0; for (size_t i = 9600; i < o.L.size(); ++i) diff = std::max (diff, std::abs (o.L[i] - o.R[i]));
        Params q = p; q.stereoPhaseDeg = 0; auto o2 = run (q, 200, 0.5, 96000);
        double diff2 = 0; for (size_t i = 9600; i < o2.L.size(); ++i) diff2 = std::max (diff2, std::abs (o2.L[i] - o2.R[i]));
        std::printf ("      L-R difference: 180 deg %.4f, 0 deg %.6f\n", diff, diff2);
        CHECK (diff > 0.01 && diff2 < 1e-9, "stereo phase offsets the right channel");
    }
    // 5. random with a seed is repeatable; Morph at 0 % is transparent
    {
        Params p; p.analogOn = false; p.amount = 0.6; p.amountMult = 4; p.rndMode = RndAmountSpeed; p.rndSeed = 1234;
        Engine a, b; a.prepare (FS); b.prepare (FS); a.setParams (p); b.setParams (p); a.reset(); b.reset();
        double maxd = 0;
        for (int blk = 0; blk < 200; ++blk)
        {
            a.setTransport (true, 0, 120, blk * 256LL); b.setTransport (true, 0, 120, blk * 256LL);
            for (int i = 0; i < 256; ++i) { double v = std::sin (0.05 * (blk * 256 + i)); double l1 = v, r1 = v, l2 = v, r2 = v; a.process (l1, r1); b.process (l2, r2); maxd = std::max (maxd, std::abs (l1 - l2)); }
        }
        CHECK (maxd < 1e-12, "seeded random: two instances stay identical");
        Params m; m.mix = 0.0; m.amount = 1.0; m.amountMult = 16;
        auto o = run (m, 1000, 0.5, 48000);
        Params d; d.amount = 0; d.analogOn = false; auto od = run (d, 1000, 0.5, 48000);
        double e = 0; for (size_t i = 4800; i < o.L.size(); ++i) e = std::max (e, std::abs (o.L[i] - od.L[i]));
        std::printf ("      morph at 0%%: max difference from a clean delay %.5f\n", e);
        CHECK (e < 0.02, "morph mix at 0% is (almost) transparent");
    }
    // 6. analog cassette mode rolls off the highs; saturation 200% distorts
    {
        Params c; c.amount = 0; c.model = 2; c.noiseGainDb = -96;
        auto o = run (c, 14000, 0.1, 48000);
        const double g = 20 * std::log10 (gz (o.L, 4800, 14000) / 0.1);
        std::printf ("      cassette model at 14 kHz: %.1f dB\n", g);
        CHECK (g < -6.0, "cassette model band-limits");
        Params s1 = c; s1.model = 0; s1.saturation = 1.0; Params s2 = s1; s2.saturation = 2.0;
        auto h = [&] (const Params& q) { auto oo = run (q, 500, 0.5, 48000); return gz (oo.L, 4800, 1500) / gz (oo.L, 4800, 500); };
        const double h1 = h (s1), h2 = h (s2);
        std::printf ("      H3/H1: saturation 100%% %.4f, 200%% %.4f\n", h1, h2);
        CHECK (h2 > h1 * 3.0, "saturation above 100% intensifies");
    }
    std::printf ("%s\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return fails;
}
