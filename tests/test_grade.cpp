#include "SampleGrade/GradeEngine.h"
#include <cstdio>
#include <vector>
using namespace grade;
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++fails; } else std::printf("ok:   %s\n", m); } while (0)
static const double FS = 192000.0;
static std::vector<double> run (const Params& p, double f, double a, int n)
{
    Engine e; e.prepare (FS); e.setParams (p); e.reset();
    std::vector<double> o;
    for (int i = 0; i < n; ++i) { double l = a * std::sin (2 * kPi * f * i / FS), r = l; e.process (l, r); o.push_back (l); }
    return o;
}
static double gz (const std::vector<double>& x, size_t from, double f)
{
    double w = 2 * kPi * f / FS, c = 2 * std::cos (w), s1 = 0, s2 = 0; size_t n = x.size() - from;
    for (size_t i = from; i < x.size(); ++i) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (n * 0.5);
}
int main()
{
    CHECK (std::abs (kGrades[0].rate - 31250.0) < 1e-9 && std::abs (kGrades[3].rate - 3906.25) < 1e-9, "grade rates derived from a 16 MHz clock");
    { Params p; p.grade = 0; p.codec = 0; p.noiseDb = -120; auto o = run (p, 1000, 0.5, 96000);
      const double g = gz (o, 19200, 1000) / 0.5; std::printf ("      hi-fi linear 1 kHz gain %.3f\n", g);
      CHECK (g > 0.85 && g < 1.1, "Hi-Fi linear passes 1 kHz"); }
    { Params p; p.grade = 0; p.codec = 0; p.noiseDb = -120; auto a = run (p, 1000, 0.5, 96000);
      p.codec = 1; auto b = run (p, 1000, 0.5, 96000);
      double e = 0, s = 0; for (size_t i = 19200; i < a.size(); ++i) { e += (a[i] - b[i]) * (a[i] - b[i]); s += a[i] * a[i]; }
      const double snr = 10 * std::log10 (s / e); std::printf ("      4-bit ADPCM SNR vs linear: %.1f dB\n", snr);
      CHECK (snr > 12 && snr < 45, "ADPCM adds audible but bounded codec noise"); }
    { Params p; p.grade = 2; p.codec = 0; p.outFilter = 2; p.noiseDb = -120; auto o = run (p, 1000, 0.5, 96000);
      const double img = gz (o, 19200, 7812.5 - 1000) / gz (o, 19200, 1000); std::printf ("      Lo-Fi 1 image at 6.8 kHz: %.3f of the fundamental\n", img);
      CHECK (img > 0.05, "Lo-Fi 1 hold produces images above the band"); }
    { Params p; p.grade = 3; p.codec = 0; p.noiseDb = -120; p.outFilter = 2;
      p.decim = 0; auto c = run (p, 3000, 0.5, 96000); p.decim = 2; auto r = run (p, 3000, 0.5, 96000);
      const double al = 3906.25 - 3000; const double ac = gz (c, 19200, al), ar = gz (r, 19200, al);
      std::printf ("      Lo-Fi 2, 3 kHz in: alias at 906 Hz clean %.4f, raw %.4f\n", ac, ar);
      CHECK (ar > 0.1 && ac < ar * 0.2, "Lo-Fi 2: raw decimation aliases, clean filter rejects"); }
    { Params p; p.inGainDb = 24; auto o = run (p, 200, 0.9, 48000); double pk = 0; for (double v : o) pk = std::max (pk, std::abs (v));
      std::printf ("      peak with +24 dB drive: %.2f\n", pk); CHECK (pk < 1.6 && std::isfinite (pk), "converter clipping keeps output bounded"); }
    std::printf ("%s\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return fails;
}
