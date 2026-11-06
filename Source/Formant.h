#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace aurora
{
// Moves three vocal-style resonances up or down: cuts around 500/1500/2800 Hz
// and boosts the same bands shifted by `semis`. It is an approximation of a
// formant shift (it colours the sound rather than re-warping the spectral
// envelope), but it gives the familiar "bigger / smaller / alien" character.
class FormantFilter
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& ch : filt)
            for (auto& f : ch)
                f.reset();
        current = 1000.0f; // force first update
        setAmount (0.0f);
    }

    void setAmount (float semis)
    {
        if (std::abs (semis - current) < 0.005f)
            return;
        current = semis;

        const float depthDb = juce::jmin (1.0f, std::abs (semis) / 6.0f) * 7.0f;
        const float ratio = std::exp2 (semis / 12.0f);
        constexpr float q = 1.4f;
        const float nyq = 0.45f * (float) sampleRate;
        static constexpr std::array<float, 3> f0 { 500.0f, 1500.0f, 2800.0f };

        for (size_t i = 0; i < 3; ++i)
        {
            auto cut = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, f0[i], q, juce::Decibels::decibelsToGain (-depthDb));
            auto boost = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, juce::jlimit (80.0f, nyq, f0[i] * ratio), q, juce::Decibels::decibelsToGain (depthDb));
            for (auto& ch : filt)
            {
                ch[2 * i].coefficients = cut;
                ch[2 * i + 1].coefficients = boost;
            }
        }
    }

    float process (int ch, float x)
    {
        for (auto& f : filt[(size_t) ch])
            x = f.processSample (x);
        return x;
    }

private:
    std::array<std::array<juce::dsp::IIR::Filter<float>, 6>, 2> filt;
    double sampleRate = 44100.0;
    float current = 1000.0f;
};
} // namespace aurora
