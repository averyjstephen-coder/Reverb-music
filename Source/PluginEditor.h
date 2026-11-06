#pragma once
#include "PluginProcessor.h"

//==============================================================================
class AuroraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuroraLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
// A labelled rotary knob bound to a parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& name,
          const juce::String& suffix, int decimals, juce::Colour accent, double defaultValue);

    void resized() override;

    juce::Slider slider;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class AuroraEditor : public juce::AudioProcessorEditor
{
public:
    explicit AuroraEditor (AuroraProcessor&);
    ~AuroraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    AuroraProcessor& proc;
    AuroraLookAndFeel laf;

    static inline const juce::Colour teal { 0xff5ef0d0 };
    static inline const juce::Colour violet { 0xffa78bfa };

    // Shift
    Knob pitch, formant, shiftMix;
    juce::ToggleButton snapButton { "Snap to semitones" };
    std::unique_ptr<ButtonAtt> snapAtt;

    // Space
    Knob decay, size, preDelay, damp, lowCut, mod, revMix;
    juce::Label colorLabel;
    juce::ComboBox colorBox;
    std::unique_ptr<ComboAtt> colorAtt;

    // Footer
    juce::Label routingLabel, outputLabel;
    juce::ComboBox routingBox;
    std::unique_ptr<ComboAtt> routingAtt;
    juce::Slider outputSlider;
    std::unique_ptr<SliderAtt> outputAtt;

    juce::Rectangle<int> headerArea, leftPanel, rightPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraEditor)
};
