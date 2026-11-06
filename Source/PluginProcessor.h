#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Core.h"
#include "Formant.h"

class AuroraProcessor : public juce::AudioProcessor
{
public:
    AuroraProcessor();
    ~AuroraProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Aurora"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    aurora::GranularShifter shifter[2];
    aurora::FormantFilter formant;
    aurora::VintageReverb reverb;

    juce::SmoothedValue<float> pitchSm, shiftMixSm, revMixSm, outSm;

    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pSnap = nullptr;
    std::atomic<float>* pShiftMix = nullptr;
    std::atomic<float>* pOrder = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pPreDelay = nullptr;
    std::atomic<float>* pDamp = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pMod = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pRevMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraProcessor)
};
