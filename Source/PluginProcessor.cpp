#include "PluginProcessor.h"
#include "PluginEditor.h"

AuroraProcessor::AuroraProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AuroraState", createLayout())
{
    pPitch = apvts.getRawParameterValue ("pitch");
    pFormant = apvts.getRawParameterValue ("formant");
    pSnap = apvts.getRawParameterValue ("snap");
    pShiftMix = apvts.getRawParameterValue ("shiftMix");
    pOrder = apvts.getRawParameterValue ("order");
    pDecay = apvts.getRawParameterValue ("decay");
    pSize = apvts.getRawParameterValue ("size");
    pPreDelay = apvts.getRawParameterValue ("preDelay");
    pDamp = apvts.getRawParameterValue ("damp");
    pLowCut = apvts.getRawParameterValue ("lowCut");
    pMod = apvts.getRawParameterValue ("mod");
    pColor = apvts.getRawParameterValue ("color");
    pRevMix = apvts.getRawParameterValue ("revMix");
    pOutput = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout AuroraProcessor::createLayout()
{
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name, NR range, float def, const char* unit)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    // Shift section
    addFloat ("pitch", "Pitch", NR (-12.0f, 12.0f, 0.01f), 0.0f, "st");
    addFloat ("formant", "Formant", NR (-12.0f, 12.0f, 0.01f), 0.0f, "st");
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "snap", 1 }, "Snap", false));
    addFloat ("shiftMix", "Shift Mix", NR (0.0f, 100.0f, 0.1f), 100.0f, "%");

    // Space section
    addFloat ("decay", "Decay", NR (0.1f, 20.0f, 0.01f, 0.4f), 2.5f, "s");
    addFloat ("size", "Size", NR (0.0f, 100.0f, 0.1f), 60.0f, "%");
    addFloat ("preDelay", "Pre-Delay", NR (0.0f, 250.0f, 0.1f, 0.5f), 15.0f, "ms");
    addFloat ("damp", "Damp", NR (1000.0f, 20000.0f, 1.0f, 0.4f), 9000.0f, "Hz");
    addFloat ("lowCut", "Low Cut", NR (20.0f, 1000.0f, 1.0f, 0.4f), 60.0f, "Hz");
    addFloat ("mod", "Mod", NR (0.0f, 100.0f, 0.1f), 30.0f, "%");
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "color", 1 }, "Color", juce::StringArray { "Warm", "Lush", "Clear" }, 1));
    addFloat ("revMix", "Reverb Mix", NR (0.0f, 100.0f, 0.1f), 25.0f, "%");

    // Global
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "order", 1 }, "Routing", juce::StringArray { "Shift > Verb", "Shift Tail" }, 0));
    addFloat ("output", "Output", NR (-24.0f, 12.0f, 0.1f), 0.0f, "dB");

    return layout;
}

bool AuroraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

void AuroraProcessor::prepareToPlay (double sampleRate, int)
{
    shifter[0].prepare (sampleRate);
    shifter[1].prepare (sampleRate);
    formant.prepare (sampleRate);
    reverb.prepare (sampleRate);

    auto init = [sampleRate] (juce::SmoothedValue<float>& s, float v)
    {
        s.reset (sampleRate, 0.03);
        s.setCurrentAndTargetValue (v);
    };

    float pitch = pPitch->load();
    if (pSnap->load() > 0.5f) pitch = std::round (pitch);
    init (pitchSm, pitch);
    init (shiftMixSm, pShiftMix->load() * 0.01f);
    init (revMixSm, pRevMix->load() * 0.01f);
    init (outSm, juce::Decibels::decibelsToGain (pOutput->load()));
}

void AuroraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    float pitch = pPitch->load();
    if (pSnap->load() > 0.5f)
        pitch = std::round (pitch);
    pitchSm.setTargetValue (pitch);
    shiftMixSm.setTargetValue (pShiftMix->load() * 0.01f);
    revMixSm.setTargetValue (pRevMix->load() * 0.01f);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    formant.setAmount (pFormant->load());

    aurora::VintageReverb::Params rp;
    rp.decay = pDecay->load();
    rp.size = pSize->load() * 0.01f;
    rp.preDelayMs = pPreDelay->load();
    rp.dampHz = pDamp->load();
    rp.lowCutHz = pLowCut->load();
    rp.mod = pMod->load() * 0.01f;
    rp.color = (int) pColor->load();
    reverb.setParams (rp);

    const bool tailMode = pOrder->load() > 0.5f;

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i];
        const float inR = r != nullptr ? r[i] : inL;

        const float ratio = std::exp2 (pitchSm.getNextValue() * (1.0f / 12.0f));
        shifter[0].setRatio (ratio);
        shifter[1].setRatio (ratio);
        const float sm = shiftMixSm.getNextValue();
        const float rm = revMixSm.getNextValue();
        const float og = outSm.getNextValue();

        auto shiftSection = [&] (float a, float b, float& oa, float& ob)
        {
            const float sa = formant.process (0, shifter[0].process (a));
            const float sb = formant.process (1, shifter[1].process (b));
            oa = a + sm * (sa - a);
            ob = b + sm * (sb - b);
        };

        float outL, outR;
        if (! tailMode)
        {
            // Voice is shifted, then sent into the reverb
            float sL, sR, wL, wR;
            shiftSection (inL, inR, sL, sR);
            reverb.process (sL, sR, wL, wR);
            outL = sL * (1.0f - rm) + wL * rm;
            outR = sR * (1.0f - rm) + wR * rm;
        }
        else
        {
            // Dry voice stays natural; only the reverb tail is shifted
            float wL, wR, tL, tR;
            reverb.process (inL, inR, wL, wR);
            shiftSection (wL, wR, tL, tR);
            outL = inL * (1.0f - rm) + tL * rm;
            outR = inR * (1.0f - rm) + tR * rm;
        }

        l[i] = outL * og;
        if (r != nullptr)
            r[i] = outR * og;
    }
}

juce::AudioProcessorEditor* AuroraProcessor::createEditor()
{
    return new AuroraEditor (*this);
}

void AuroraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuroraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuroraProcessor();
}
