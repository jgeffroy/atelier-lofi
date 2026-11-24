#pragma once

// Small DSP building blocks for RetroDeck (independent of JUCE).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace retro
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (g, 1e-9)); }

struct Rng
{
    uint32_t s = 0x1234567u;
    double next()      { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) s / 2147483647.5 - 1.0; } // [-1, 1]
    double uniform()   { return 0.5 * (next() + 1.0); }                                                      // [0, 1]
};

// RBJ biquad, transposed direct form II
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    void identity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    static double clampF (double fs, double f) { return std::clamp (f, 5.0, fs * 0.45); }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    void lowpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double fs, double f, double q)
    {
        const double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

struct OnePoleLP
{
    double a = 1, z = 0;
    void set (double fs, double f) { a = 1.0 - std::exp (-2 * kPi * std::clamp (f, 1.0, fs * 0.45) / fs); }
    void reset() { z = 0; }
    double process (double x) { z += (x - z) * a; return z; }
};

// Envelope follower with separate attack and release (seconds)
struct Envelope
{
    double att = 0.01, rel = 0.1, e = 0;
    void set (double fs, double attackS, double releaseS)
    {
        att = 1.0 - std::exp (-1.0 / (std::max (attackS, 1e-5) * fs));
        rel = 1.0 - std::exp (-1.0 / (std::max (releaseS, 1e-5) * fs));
    }
    void reset() { e = 0; }
    double process (double x) { const double a = std::abs (x); e += (a - e) * (a > e ? att : rel); return e; }
};

struct DcBlocker
{
    double x1 = 0, y1 = 0, r = 0.999;
    void set (double fs) { r = 1.0 - 2 * kPi * 10.0 / fs; }
    void reset() { x1 = y1 = 0; }
    double process (double x) { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// Delay line with 4-point Hermite interpolation; read(d) gives the sample d samples ago.
struct DelayBuf
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void init (int minSize)
    {
        int n = 16;
        while (n < minSize + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    int capacity() const { return mask + 1; }
    void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (double d) const
    {
        d = std::clamp (d, 1.0, (double) (mask - 4));
        const int i0 = (int) d;
        const float t = (float) (d - i0);
        const float xm1 = buf[(size_t) ((w - i0) & mask)];
        const float x0  = buf[(size_t) ((w - i0 - 1) & mask)];
        const float x1  = buf[(size_t) ((w - i0 - 2) & mask)];
        const float x2  = buf[(size_t) ((w - i0 - 3) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
};

// Antiderivative anti-aliased (ADAA) saturators: smooth (biased tanh) and hard (cubic soft clip).
struct AdaaSat
{
    double x1 = 0, F1 = 0; bool first = true;
    void reset() { x1 = 0; F1 = 0; first = true; }

    static double logcosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }

    static double fSmooth (double x, double b) { return std::tanh (x + b) - std::tanh (b); }
    static double FSmooth (double x, double b) { return logcosh (x + b) - std::tanh (b) * x; }

    static double fHard (double x)
    {
        const double u = x / 1.5;
        return std::abs (u) < 1.0 ? 1.5 * (u - u * u * u / 3.0) : (u > 0 ? 1.0 : -1.0);
    }
    static double FHard (double x)
    {
        const double u = x / 1.5, au = std::abs (u);
        const double H = au < 1.0 ? (u * u / 2.0 - u * u * u * u / 12.0) : (2.0 / 3.0 * au - 0.25);
        return 2.25 * H;
    }

    // Only the non-linear residual goes through the ADAA averaging, so the clean (linear) part keeps
    // a flat frequency response while the distortion products are still anti-aliased.
    double process (double x, bool hard, double bias)
    {
        const double F = hard ? FHard (x) : FSmooth (x, bias);
        const double k = hard ? 1.0 : 1.0 - std::tanh (bias) * std::tanh (bias);   // small-signal slope
        double y;
        if (first) { y = hard ? fHard (x) : fSmooth (x, bias); first = false; x1 = x; }
        else if (std::abs (x - x1) > 1e-6) y = (F - F1) / (x - x1);
        else { const double m = 0.5 * (x + x1); y = hard ? fHard (m) : fSmooth (m, bias); }
        const double out = k * x + (y - k * 0.5 * (x + x1));
        x1 = x; F1 = F;
        return out;
    }
};

// Smoothed random LFO (random walk through new targets at a given rate), output about [-1, 1]
struct RandomLfo
{
    Rng rng; double cur = 0, target = 0, phase = 1.0, smooth = 0;
    double next (double rate, double fs)
    {
        phase += rate / fs;
        if (phase >= 1.0) { phase -= 1.0; target = rng.next(); }
        cur += (target - cur) * std::min (1.0, rate * 6.0 / fs);
        smooth += (cur - smooth) * std::min (1.0, rate * 6.0 / fs);
        return smooth;
    }
};
} // namespace retro
