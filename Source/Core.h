#pragma once
// Aurora DSP core: std-only (no JUCE) so it can be unit-tested anywhere.
#include <algorithm>
#include <cmath>
#include <vector>

namespace aurora
{
constexpr float kTwoPi = 6.28318530717958647692f;

//==============================================================================
// Power-of-two circular delay line. After push(x), readInt(1) returns x.
struct DelayLine
{
    void prepare (int maxDelaySamples)
    {
        int n = 16;
        while (n < maxDelaySamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }

    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    inline void push (float x)
    {
        buf[(size_t) w] = x;
        w = (w + 1) & mask;
    }

    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }

    // Linear interpolation, d >= 1
    inline float read (float d) const
    {
        const int di = (int) d;
        const float frac = d - (float) di;
        const float a = buf[(size_t) ((w - di) & mask)];
        const float b = buf[(size_t) ((w - di - 1) & mask)];
        return a + frac * (b - a);
    }

    // 4-point Catmull-Rom, d >= 4
    inline float readHermite (float d) const
    {
        const int di = (int) d;
        const float frac = d - (float) di;
        const float p0 = buf[(size_t) ((w - di - 2) & mask)]; // oldest
        const float p1 = buf[(size_t) ((w - di - 1) & mask)];
        const float p2 = buf[(size_t) ((w - di) & mask)];
        const float p3 = buf[(size_t) ((w - di + 1) & mask)]; // newest
        const float u = 1.0f - frac;
        const float c1 = 0.5f * (p2 - p0);
        const float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
        return ((c3 * u + c2) * u + c1) * u + p1;
    }

    std::vector<float> buf;
    int mask = 0, w = 0;
};

//==============================================================================
// Two-tap crossfading delay-line pitch shifter (classic "rotating tape head").
class GranularShifter
{
public:
    void prepare (double sampleRate)
    {
        window = (float) (0.045 * sampleRate); // ~45 ms grains
        line.prepare ((int) window + 16);
        reset();
    }

    void reset() { line.clear(); phase = 0.0f; }

    void setRatio (float r) { inc = (1.0f - r) / window; }

    float process (float x)
    {
        line.push (x);
        float out = 0.0f;
        for (int t = 0; t < 2; ++t)
        {
            float ph = phase + 0.5f * (float) t;
            if (ph >= 1.0f) ph -= 1.0f;
            const float g = 0.5f - 0.5f * std::cos (kTwoPi * ph);
            out += g * line.readHermite (4.0f + ph * window);
        }
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        else if (phase < 0.0f) phase += 1.0f;
        return out;
    }

private:
    DelayLine line;
    float window = 2000.0f, phase = 0.0f, inc = 0.0f;
};

//==============================================================================
// Stereo FDN reverb with input diffusion, early reflections and modulation.
class VintageReverb
{
public:
    struct Params
    {
        float decay = 2.5f;       // seconds (RT60)
        float size = 0.6f;        // 0..1
        float preDelayMs = 15.0f; // ms
        float dampHz = 9000.0f;   // high-frequency damping
        float lowCutHz = 60.0f;   // wet low cut
        float mod = 0.3f;         // 0..1
        int color = 1;            // 0 warm, 1 lush, 2 clear
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        scale = (float) (sr / 44100.0);
        for (int i = 0; i < 8; ++i)
        {
            lines[i].prepare ((int) (kBase[i] * 2.3f * scale) + 64);
            phase[i] = (float) i / 8.0f;
            lfoInc[i] = kRate[i] / (float) sr;
        }
        const int pre = (int) (0.3 * sr);
        preL.prepare (pre); preR.prepare (pre);
        const int er = (int) (0.12 * sr);
        erL.prepare (er); erR.prepare (er);
        for (int i = 0; i < 4; ++i)
        {
            apL[i].prepare ((int) (kApL[i] * scale));
            apR[i].prepare ((int) (kApR[i] * scale));
        }
        reset();
        setParams (Params {});
        sizeCur = sizeTarget;
        preCur = preTarget;
        updateCoefs();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& a : apL) a.line.clear();
        for (auto& a : apR) a.line.clear();
        preL.clear(); preR.clear(); erL.clear(); erR.clear();
        for (int i = 0; i < 8; ++i) lp[i] = 0.0f;
        lcL = lcR = 0.0f;
    }

    void setParams (const Params& np)
    {
        p = np;
        sizeTarget = p.size;
        preTarget = std::max (1.0f, p.preDelayMs * 0.001f * (float) sampleRate);
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        sizeCur += 0.0008f * (sizeTarget - sizeCur);
        preCur += 0.0008f * (preTarget - preCur);
        if (++ctr >= 16) { ctr = 0; updateCoefs(); }

        // Pre-delay
        preL.push (inL); preR.push (inR);
        const float xl = preL.read (preCur);
        const float xr = preR.read (preCur);

        // Early reflections
        erL.push (xl); erR.push (xr);
        float eL = 0.0f, eR = 0.0f;
        const float tScale = (0.5f + sizeCur) * (float) sampleRate * 0.001f;
        for (int k = 0; k < 6; ++k)
        {
            const float t = kErMs[k] * tScale;
            const float t2 = t * 1.09f;
            if (k & 1) { eL += kErG[k] * erR.read (t); eR += kErG[k] * erL.read (t2); }
            else       { eL += kErG[k] * erL.read (t); eR += kErG[k] * erR.read (t2); }
        }

        // Input diffusion
        float dl = xl, dr = xr;
        for (auto& a : apL) dl = a.process (dl);
        for (auto& a : apR) dr = a.process (dr);

        // FDN
        float o[8], v[8];
        float sum = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            phase[i] += lfoInc[i];
            if (phase[i] >= 1.0f) phase[i] -= 1.0f;
            const float d = dly[i] + std::sin (kTwoPi * phase[i]) * modDepth;
            o[i] = lines[i].read (std::max (1.0f, d));
            lp[i] = o[i] * (1.0f - dampA) + lp[i] * dampA;
            v[i] = g[i] * lp[i];
            sum += v[i];
        }
        const float s = sum * 0.25f; // Householder: 2/N with N = 8
        for (int i = 0; i < 8; ++i)
        {
            const float in = 0.4f * ((i & 1) ? dr : dl);
            lines[i].push (in + v[i] - s);
        }

        float wl = 0.5f * (o[0] - o[2] + o[4] - o[6]);
        float wr = 0.5f * (o[1] - o[3] - o[5] + o[7]);
        const auto& cp = kColors[std::clamp (p.color, 0, 2)];
        wl += cp.early * eL;
        wr += cp.early * eR;

        // Wet low cut (one-pole high-pass)
        lcL = (1.0f - lcA) * wl + lcA * lcL;
        lcR = (1.0f - lcA) * wr + lcA * lcR;
        outL = (wl - lcL) * kWetTrim;
        outR = (wr - lcR) * kWetTrim;
    }

private:
    struct AllPass
    {
        DelayLine line;
        int len = 1;
        float g = 0.6f;
        void prepare (int l) { len = std::max (1, l); line.prepare (len + 4); }
        float process (float x)
        {
            const float d = line.readInt (len);
            const float w = x + g * d;
            line.push (w);
            return d - g * w;
        }
    };

    struct ColorSet { float dampMul, diffusion, early, modMul; };

    void updateCoefs()
    {
        const auto& cp = kColors[std::clamp (p.color, 0, 2)];
        const float sz = 0.4f + 1.6f * sizeCur;
        const float decay = std::max (0.1f, p.decay);
        for (int i = 0; i < 8; ++i)
        {
            dly[i] = kBase[i] * scale * sz;
            g[i] = std::pow (10.0f, -3.0f * dly[i] / (decay * (float) sampleRate));
        }
        const float fc = std::clamp (p.dampHz * cp.dampMul, 800.0f, 0.45f * (float) sampleRate);
        dampA = std::exp (-kTwoPi * fc / (float) sampleRate);
        lcA = std::exp (-kTwoPi * std::clamp (p.lowCutHz, 20.0f, 1000.0f) / (float) sampleRate);
        modDepth = p.mod * cp.modMul * 8.0f * scale;
        for (auto& a : apL) a.g = cp.diffusion;
        for (auto& a : apR) a.g = cp.diffusion;
    }

    static constexpr float kWetTrim = 0.6f;
    static constexpr float kBase[8] = { 1327, 1637, 1811, 1931, 2347, 2647, 2963, 3499 };
    static constexpr float kRate[8] = { 0.13f, 0.17f, 0.21f, 0.27f, 0.31f, 0.37f, 0.41f, 0.47f };
    static constexpr int kApL[4] = { 142, 107, 379, 277 };
    static constexpr int kApR[4] = { 149, 113, 389, 283 };
    static constexpr float kErMs[6] = { 7.0f, 11.0f, 17.0f, 23.0f, 31.0f, 41.0f };
    static constexpr float kErG[6] = { 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.25f };
    static constexpr ColorSet kColors[3] = {
        { 0.55f, 0.55f, 0.15f, 1.4f }, // warm
        { 1.00f, 0.72f, 0.25f, 1.0f }, // lush
        { 1.40f, 0.65f, 0.45f, 0.5f }  // clear
    };

    Params p;
    double sampleRate = 44100.0;
    float scale = 1.0f;
    DelayLine lines[8], preL, preR, erL, erR;
    AllPass apL[4], apR[4];
    float phase[8] {}, lfoInc[8] {}, lp[8] {}, dly[8] {}, g[8] {};
    float sizeCur = 0.6f, sizeTarget = 0.6f, preCur = 600.0f, preTarget = 600.0f;
    float dampA = 0.5f, lcA = 0.99f, modDepth = 0.0f, lcL = 0.0f, lcR = 0.0f;
    int ctr = 0;
};
} // namespace aurora
