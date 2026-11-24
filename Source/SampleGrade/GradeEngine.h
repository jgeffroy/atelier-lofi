#pragma once

// SampleGrade engine: emulation of the sampling grades of a late-90s phrase sampler
// (original DSP, independent of JUCE). Runs at an oversampled rate (host rate x 4) so that the
// zero-order-hold images of the low grades reach the output filter instead of folding back.
//
// Chain: input stage (op-amp soft clip, ADC hard clip) -> anti-alias / decimation filter
//        -> sample at the grade rate (derived from a 16 MHz clock) -> 16-bit quantiser
//        -> optional 4-bit (or 3-bit) ADPCM codec -> zero-order hold -> reconstruction filter
//        -> output stage noise

#include "../RetroDSP.h"
#include <array>

namespace grade
{
using retro::Biquad;
using retro::Rng;
using retro::kPi;
using retro::dbToGain;

struct GradeDef { const char* name; double rate; };
static const GradeDef kGrades[] =
{
    { "Hi-Fi 31.25 kHz",            16.0e6 / 512.0  },
    { "Standard / Long 15.63 kHz",  16.0e6 / 1024.0 },
    { "Lo-Fi 1 7.81 kHz",           16.0e6 / 2048.0 },
    { "Lo-Fi 2 3.91 kHz",           16.0e6 / 4096.0 },
    { "Later SP Standard 44.1 kHz", 44100.0 },
    { "Later SP Long 22.05 kHz",    22050.0 },
    { "Later SP Lo-Fi 11.025 kHz",  11025.0 },
};
constexpr int kNumGrades = (int) (sizeof (kGrades) / sizeof (kGrades[0]));

// IMA-style ADPCM (generic 4-bit adaptive codec; 3-bit variant uses a coarser code)
struct Adpcm
{
    static constexpr int kSteps[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
        5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
        27086, 29794, 32767 };
    int pred = 0, index = 0;
    void reset() { pred = 0; index = 0; }

    // encodes one 16-bit sample and returns the decoded value (what the sampler plays back)
    int process (int sample, int bits)
    {
        const int step = kSteps[index];
        int diff = sample - pred;
        int code = 0;
        if (diff < 0) { code = 8; diff = -diff; }
        int delta = step >> 3;
        if (bits >= 4)
        {
            if (diff >= step)        { code |= 4; diff -= step;        delta += step; }
            if (diff >= step >> 1)   { code |= 2; diff -= step >> 1;   delta += step >> 1; }
            if (diff >= step >> 2)   { code |= 1;                      delta += step >> 2; }
        }
        else
        {
            if (diff >= step)        { code |= 4; diff -= step;        delta += step; }
            if (diff >= step >> 1)   { code |= 2;                      delta += step >> 1; }
        }
        pred += (code & 8) ? -delta : delta;
        pred = std::clamp (pred, -32768, 32767);
        static const int idx4[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
        static const int idx3[8] = { -1, -1, 1, 1, 2, 2, 4, 4 };
        index += bits >= 4 ? idx4[code & 7] : idx3[code & 6];
        index = std::clamp (index, 0, 88);
        return pred;
    }
};

struct Params
{
    int    grade = 1;
    int    codec = 1;          // 0 linear 16-bit, 1 ADPCM 4-bit, 2 ADPCM 3-bit
    int    decim = 1;          // 0 clean (steep), 1 simple (2-pole), 2 raw (no decimation filter)
    int    outFilter = 0;      // 0 fixed hardware-style, 1 tracks the grade, 2 off
    double outCutHz = 13000.0;
    double inGainDb = 0.0;     // drive into the input stage / ADC
    double noiseDb = -86.0;    // output stage noise floor (-120 = off)
    double bitsOverride = 16;  // 16 = normal; lower values quantise more coarsely before the codec
    bool   mono = false;
};

class Engine
{
public:
    void prepare (double oversampledRate)
    {
        fs = oversampledRate;
        for (int c = 0; c < 2; ++c) rng[c].s = 0x2545F491u + (uint32_t) c * 7777u;
        reset();
        setParams (p);
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& b : aa[c])  b.reset();
            for (auto& b : dec[c]) b.reset();
            for (auto& b : rec[c]) b.reset();
            codec[c].reset();
            held[c] = 0; prevIn[c] = 0;
        }
        phase = 0;
    }

    void setParams (const Params& np)
    {
        const bool gradeChanged = np.grade != p.grade || np.codec != p.codec;
        p = np;
        const double gr = kGrades[std::clamp (p.grade, 0, kNumGrades - 1)].rate;
        rate = gr;

        // ADC front end: a steep (delta-sigma style) filter for the highest rate of the original machine
        static const double q8[4] = { 0.5098, 0.6013, 0.8999, 2.5629 };
        const double hiFiNyq = 31250.0 * 0.45;   // ADC digital filter of the original machine
        for (int c = 0; c < 2; ++c)
        {
            for (int k = 0; k < 4; ++k) aa[c][(size_t) k].lowpass (fs, gr > 31250.0 ? gr * 0.45 : hiFiNyq, q8[k]);
            // decimation filter for the lower grades
            for (int k = 0; k < 4; ++k)
            {
                if (p.decim == 0)                    dec[c][(size_t) k].lowpass (fs, gr * 0.45, q8[k]);
                else if (p.decim == 1 && k == 0)     dec[c][(size_t) k].lowpass (fs, gr * 0.5, 0.7071);
                else                                 dec[c][(size_t) k].identity();
            }
            // reconstruction filter after the DAC (3rd-order style: one biquad + one pole)
            const double cut = p.outFilter == 0 ? p.outCutHz : std::min (p.outCutHz, gr * 0.45);
            if (p.outFilter == 2) { rec[c][0].identity(); rec[c][1].identity(); }
            else { rec[c][0].lowpass (fs, cut, 1.0); rec[c][1].lowpass (fs, cut * 1.2, 0.55); }
        }
        if (gradeChanged) for (auto& c : codec) c.reset();
        noise = p.noiseDb <= -119.0 ? 0.0 : dbToGain (p.noiseDb);
        quantStep = std::pow (2.0, 16.0 - std::clamp (p.bitsOverride, 4.0, 16.0));
    }

    void process (double& l, double& r)
    {
        const double g = dbToGain (p.inGainDb);
        double x[2] = { l * g, r * g };
        if (p.mono) { const double m = 0.5 * (x[0] + x[1]); x[0] = x[1] = m; }

        double f[2];
        for (int c = 0; c < 2; ++c)
        {
            // input op-amp (soft) then converter full scale (hard)
            double v = x[c];
            v = std::abs (v) > 0.8 ? (v > 0 ? 1.0 : -1.0) * (0.8 + 0.2 * std::tanh ((std::abs (v) - 0.8) / 0.2)) : v;
            v = std::clamp (v, -1.0, 1.0);
            for (auto& b : aa[c]) v = b.process (v);
            for (auto& b : dec[c]) v = b.process (v);
            f[c] = v;
        }

        // sampling instants at the grade rate (linear interpolation at the exact instant)
        phase += rate / fs;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            const double frac = phase / (rate / fs);      // how far past the instant we are, in samples
            for (int c = 0; c < 2; ++c)
            {
                const double at = f[c] - (f[c] - prevIn[c]) * std::min (frac, 1.0);
                int s = (int) std::lround (std::clamp (at, -1.0, 1.0) * 32767.0);
                if (quantStep > 1.0) s = (int) (std::floor (s / quantStep) * quantStep);
                if (p.codec == 1) s = codec[c].process (s, 4);
                else if (p.codec == 2) s = codec[c].process (s, 3);
                held[c] = s / 32768.0;
            }
        }
        prevIn[0] = f[0]; prevIn[1] = f[1];

        double o[2];
        for (int c = 0; c < 2; ++c)
        {
            double v = held[c];
            v = rec[c][1].process (rec[c][0].process (v));
            v += rng[c].next() * noise * 1.7;
            o[c] = v;
        }
        l = o[0]; r = o[1];
    }

private:
    double fs = 192000.0, rate = 15625.0, phase = 0.0, noise = 0.0, quantStep = 1.0;
    Params p;
    std::array<std::array<Biquad, 4>, 2> aa, dec;
    std::array<std::array<Biquad, 2>, 2> rec;
    std::array<Adpcm, 2> codec;
    std::array<Rng, 2> rng;
    double held[2] = { 0, 0 }, prevIn[2] = { 0, 0 };
};
} // namespace grade
