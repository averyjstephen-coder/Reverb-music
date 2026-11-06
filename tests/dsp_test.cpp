// Standalone sanity tests for Aurora's DSP core (no JUCE needed).
// Build: g++ -std=c++17 -O2 -I../Source dsp_test.cpp -o dsp_test && ./dsp_test
#include "Core.h"
#include <cstdio>
#include <cmath>
#include <vector>

using namespace aurora;

static double estimateFreq (const std::vector<float>& x, double sr)
{
    int crossings = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f) ++crossings;
    return crossings * sr / (double) x.size();
}

static bool testShifter (double sr, double inHz, double semis)
{
    GranularShifter s;
    s.prepare (sr);
    s.setRatio (std::exp2 ((float) semis / 12.0f));
    std::vector<float> out;
    const int total = (int) (sr * 1.5);
    for (int i = 0; i < total; ++i)
    {
        const float y = s.process (std::sin (kTwoPi * (float) (inHz * i / sr)));
        if (i > (int) (sr * 0.5)) out.push_back (y);
    }
    const double f = estimateFreq (out, sr);
    const double expect = inHz * std::exp2 (semis / 12.0);
    const bool ok = std::abs (f - expect) < expect * 0.03;
    std::printf ("shifter %+5.1f st: expected %.1f Hz, measured %.1f Hz  [%s]\n", semis, expect, f, ok ? "ok" : "FAIL");
    return ok;
}

static bool testReverb (double sr, float decay, int color)
{
    VintageReverb r;
    r.prepare (sr);
    VintageReverb::Params p;
    p.decay = decay; p.size = 0.6f; p.preDelayMs = 10; p.dampHz = 12000; p.lowCutHz = 20; p.mod = 0.3f; p.color = color;
    r.setParams (p);

    const int total = (int) (sr * (decay * 3.0 + 2.0));
    std::vector<double> e ((size_t) total);
    bool finite = true;
    for (int i = 0; i < total; ++i)
    {
        float oL, oR;
        r.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, oL, oR);
        if (! std::isfinite (oL) || ! std::isfinite (oR)) finite = false;
        e[(size_t) i] = (double) oL * oL + (double) oR * oR;
    }
    // Schroeder backward integration
    std::vector<double> edc ((size_t) total);
    double acc = 0.0;
    for (int i = total - 1; i >= 0; --i) { acc += e[(size_t) i]; edc[(size_t) i] = acc; }
    const double e0 = edc[0];
    auto db = [&] (int i) { return 10.0 * std::log10 (edc[(size_t) i] / e0 + 1e-20); };
    int i5 = -1, i25 = -1;
    for (int i = 0; i < total; ++i)
    {
        if (i5 < 0 && db (i) <= -5.0) i5 = i;
        if (i25 < 0 && db (i) <= -25.0) { i25 = i; break; }
    }
    double rt60 = -1.0;
    if (i5 >= 0 && i25 >= 0) rt60 = (double) (i25 - i5) / sr * 3.0; // T20 x 3
    const bool ok = finite && e0 > 1e-9 && rt60 > decay * 0.5 && rt60 < decay * 1.6;
    std::printf ("reverb color %d decay %.1fs: energy %.3g, measured RT60 ~ %.2fs, finite=%d  [%s]\n",
                 color, decay, e0, rt60, (int) finite, ok ? "ok" : "FAIL");
    return ok;
}

int main()
{
    bool ok = true;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("--- %.0f Hz ---\n", sr);
        ok &= testShifter (sr, 440.0, 5.0);
        ok &= testShifter (sr, 440.0, -7.0);
        ok &= testShifter (sr, 440.0, 12.0);
        ok &= testShifter (sr, 440.0, -12.0);
        ok &= testReverb (sr, 1.5f, 1);
        ok &= testReverb (sr, 4.0f, 0);
    }
    std::printf ("%s\n", ok ? "ALL PASSED" : "SOME TESTS FAILED");
    return ok ? 0 : 1;
}
