// Standalone tests / measurements for the VinylSim engine
#include "../Source/VinylSim/VinylEngine.h"
#include <cstdio>
#include <vector>
#include <cmath>

using namespace vsim;
static int fails = 0;
#define CHECK(c, msg) do { if (c) std::printf ("ok   %s\n", msg); else { std::printf ("FAIL %s\n", msg); ++fails; } } while (0)

static Params clean()
{
    Params p;
    p.brilliance = 0; p.tipRes = 0.3; p.sheen = 0; p.cutLimit = 0; p.ellipticalHz = 0; p.warp = 0; p.flutter = 0;
    p.crackle = p.dust = p.pops = p.hiss = p.rumble = 0; p.autoGain = false; p.lowCutHz = 5; p.separationDb = 60;
    return p;
}

static double goertzel (const std::vector<double>& x, size_t a, size_t n, double f, double fs)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * kPi * i / (n - 1));
        re += x[a + i] * w * std::cos (2 * kPi * f * i / fs); im += x[a + i] * w * std::sin (2 * kPi * f * i / fs);
    }
    return std::sqrt (re * re + im * im) * 4.0 / n;
}

struct Run { std::vector<double> l, r; };
static Run run (const Params& p, const std::vector<double>& in, double fs = 48000)
{
    Engine e; e.prepare (fs); e.setParams (p); e.reset();
    Run o; o.l.resize (in.size()); o.r.resize (in.size());
    for (size_t i = 0; i < in.size(); ++i) { double a = in[i], b = in[i]; e.process (a, b); o.l[i] = a; o.r[i] = b; }
    return o;
}

static std::vector<double> sine (double f, double amp, double fs, double sec)
{
    std::vector<double> x ((size_t) (fs * sec));
    for (size_t i = 0; i < x.size(); ++i) x[i] = amp * std::sin (2 * kPi * f * i / fs);
    return x;
}

static double gainDb (const Params& p, double f, double fs = 48000, double amp = 0.05)
{
    auto x = sine (f, amp, fs, 0.5);
    auto o = run (p, x, fs);
    const size_t n = (size_t) (0.25 * fs), a = x.size() - n;
    return 20 * std::log10 (goertzel (o.l, a, n, f, fs) / amp + 1e-12);
}

static void harm (const Params& p, double f, double amp, double& h2, double& h3, double fs = 48000)
{
    auto x = sine (f, amp, fs, 0.6);
    auto o = run (p, x, fs);
    const size_t n = (size_t) (0.3 * fs), a = x.size() - n;
    const double f1 = goertzel (o.l, a, n, f, fs);
    h2 = 2 * f < fs * 0.45 ? 100 * goertzel (o.l, a, n, 2 * f, fs) / f1 : 0;
    h3 = 3 * f < fs * 0.45 ? 100 * goertzel (o.l, a, n, 3 * f, fs) / f1 : 0;
}

int main()
{
    const double fs = 48000;
    // ---- latency
    {
        Params p = clean(); p.mix = 1;
        Engine e; e.prepare (fs); e.setParams (p); e.reset();
        // feed a band-limited click (Hann-windowed) and look for the peak
        std::vector<double> x (4000, 0.0);
        for (int i = 0; i < 9; ++i) x[100 + i] = 0.5 * (1 - std::cos (2 * kPi * i / 8)) * 0.5;
        int best = 0; double bv = 0;
        for (size_t i = 0; i < x.size(); ++i) { double a = x[i], b = x[i]; e.process (a, b); if (std::abs (a) > bv) { bv = std::abs (a); best = (int) i; } }
        const int lat = best - 104;
        std::printf ("     latency reported %d, measured %d (peak %.3f)\n", e.latencySamples(), lat, bv);
        CHECK (std::abs (lat - e.latencySamples()) <= 2, "latency matches report");
    }
    // ---- frequency response
    {
        const double fr[] = { 30, 100, 1000, 5000, 8000, 10000, 12000, 15000, 18000, 20000 };
        auto row = [&] (const char* name, const Params& p)
        {
            std::printf ("     %-26s", name);
            for (double f : fr) std::printf ("%6.1f", gainDb (p, f));
            std::printf ("\n");
        };
        std::printf ("     %-26s", "response (dB)");
        for (double f : fr) std::printf ("%6.0f", f);
        std::printf ("\n");
        Params p = clean(); p.stylus = 0; row ("clean, no brilliance", p); p.stylus = 1;
        p.tipRes = 0.5; row ("natural tip res", p);
        for (int r = 0; r < 4; ++r) { p.brilliance = 1; p.brilRange = r; char b[64]; std::snprintf (b, 64, "brilliance 100%% x%d", 1 << r); row (b, p); }
        p = clean(); p.brilliance = 0.5; row ("brilliance 50% x1", p);
        p = clean(); p.loadPf = 500; p.tipRes = 0.5; row ("load 500 pF", p);
        p = clean(); p.cart = 1; p.tipRes = 0.5; row ("MM Bright", p);
        p = clean(); p.cart = 4; p.tipRes = 0.5; row ("Moving coil", p);
        p = clean(); p.cart = 5; p.tipRes = 0.5; row ("Ceramic", p);
        p = clean(); p.position = 1; p.stylus = 0; p.tipRes = 0.5; row ("inner groove conical", p);
        p = clean(); p.format = 3; p.tipRes = 0.5; row ("shellac 78", p);
        p = clean(); Params q = p; q.brilliance = 1; q.brilRange = 3;
        CHECK (gainDb (q, 15000) - gainDb (p, 15000) > 15.0, "brilliance x8 adds > 15 dB around the resonance");
        q.brilRange = 0;
        CHECK (gainDb (q, 15000) - gainDb (p, 15000) > 5.0, "brilliance x1 100 % adds > 5 dB");
        CHECK (std::abs (gainDb (q, 1000) - gainDb (p, 1000)) < 1.0, "brilliance leaves 1 kHz alone");
    }
    // ---- tracing distortion (H2 at -12 dBFS)
    {
        std::printf ("     tracing H2 %% at -12 dBFS        1k     4k     7k\n");
        auto row = [&] (const char* name, const Params& p)
        {
            double a, b, c, d, e2, f;
            harm (p, 1000, 0.25, a, b); harm (p, 4000, 0.25, c, d); harm (p, 7000, 0.25, e2, f);
            std::printf ("     %-28s %6.2f %6.2f %6.2f   (H3 4k %.2f)\n", name, a, c, e2, d);
            return c;
        };
        Params p = clean();
        for (int s = 0; s < 4; ++s)
        {
            p.stylus = s;
            p.position = 0; char n1[64]; std::snprintf (n1, 64, "%s outer", kStyli[s].name); row (n1, p);
            p.position = 1; char n2[64]; std::snprintf (n2, 64, "%s inner", kStyli[s].name); row (n2, p);
        }
        p = clean(); p.position = 0.3;
        const double h0 = row ("default stylus 30 %, sheen 0", p);
        p.sheen = 1; const double h1 = row ("sheen 100 % x1", p);
        p.sheenRange = 3; const double h8 = row ("sheen 100 % x8", p);
        p = clean(); p.position = 0.3; p.grooveDb = 9; row ("groove level +9 dB", p);
        p = clean(); p.position = 0.3; p.wear = 1; row ("wear 100 %", p);
        CHECK (h1 > 3.0 * h0, "sheen x1 multiplies H2 by > 3");
        CHECK (h8 > 15.0 * h0, "sheen x8 multiplies H2 by > 15");
    }
    // ---- noise floor, warp, robustness
    {
        Params p; p.mix = 1;   // defaults
        std::vector<double> sil ((size_t) (fs * 2), 0.0);
        auto o = run (p, sil);
        double s2 = 0, pk = 0; for (size_t i = (size_t) fs; i < sil.size(); ++i) { s2 += o.l[i] * o.l[i]; pk = std::max (pk, std::abs (o.l[i])); }
        std::printf ("     default surface: rms %.1f dBFS, peak %.1f dBFS\n", 10 * std::log10 (s2 / fs + 1e-20), 20 * std::log10 (pk + 1e-12));
        p.surfRange = 3; p.crackle = p.dust = p.pops = p.hiss = p.rumble = 1;
        o = run (p, sil); s2 = 0; pk = 0;
        for (size_t i = (size_t) fs; i < sil.size(); ++i) { s2 += o.l[i] * o.l[i]; pk = std::max (pk, std::abs (o.l[i])); }
        std::printf ("     max surface x8:  rms %.1f dBFS, peak %.1f dBFS\n", 10 * std::log10 (s2 / fs + 1e-20), 20 * std::log10 (pk + 1e-12));
        CHECK (pk <= 1.0, "surface x8 stays below full scale");

        Rng rng; rng.s = 77; bool finite = true; double maxPk = 0;
        for (int round = 0; round < 40; ++round)
        {
            Params q;
            q.brilliance = rng.uniform(); q.brilRange = (int) (rng.uniform() * 4); q.cart = (int) (rng.uniform() * 6);
            q.loadPf = 50 + 950 * rng.uniform(); q.loadKohm = 10 + 90 * rng.uniform(); q.tipRes = rng.uniform();
            q.sheen = rng.uniform(); q.sheenRange = (int) (rng.uniform() * 4); q.stylus = (int) (rng.uniform() * 4);
            q.format = (int) (rng.uniform() * 4); q.position = rng.uniform(); q.grooveDb = -12 + 30 * rng.uniform();
            q.wear = rng.uniform(); q.cutLimit = rng.uniform(); q.cutRange = (int) (rng.uniform() * 4);
            q.ellipticalHz = 300 * rng.uniform(); q.separationDb = 10 + 30 * rng.uniform(); q.lowCutHz = 5 + 35 * rng.uniform();
            q.warp = rng.uniform(); q.warpRange = (int) (rng.uniform() * 4); q.flutter = rng.uniform();
            q.crackle = rng.uniform(); q.dust = rng.uniform(); q.pops = rng.uniform(); q.hiss = rng.uniform(); q.rumble = rng.uniform();
            q.surfRange = (int) (rng.uniform() * 4); q.mix = rng.uniform(); q.autoGain = rng.uniform() > 0.5; q.seed = (unsigned) (rng.uniform() * 100);
            std::vector<double> x ((size_t) (fs * 0.4));
            for (size_t i = 0; i < x.size(); ++i) x[i] = 0.7 * std::sin (2 * kPi * 220 * i / fs) * std::sin (2 * kPi * 3 * i / fs) + 0.25 * rng.next();
            Engine e; e.prepare (rng.uniform() > 0.5 ? 44100 : 96000); e.setParams (q); e.reset();
            for (size_t i = 0; i < x.size(); ++i)
            {
                double a = x[i], b = -x[i]; e.process (a, b);
                if (! std::isfinite (a) || ! std::isfinite (b)) finite = false;
                maxPk = std::max (maxPk, std::max (std::abs (a), std::abs (b)));
                if (i == x.size() / 2) { q.brilliance = rng.uniform(); q.format = (int) (rng.uniform() * 4); e.setParams (q); }
            }
        }
        std::printf ("     random rounds max peak %.3f\n", maxPk);
        CHECK (finite, "random parameters: finite output");
        CHECK (maxPk <= 1.0001, "random parameters: output within full scale");
    }
    std::printf (fails ? "\n%d FAILURES\n" : "\nALL OK\n", fails);
    return fails ? 1 : 0;
}
