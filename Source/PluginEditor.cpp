#include "PluginEditor.h"

//==============================================================================
AuroraLookAndFeel::AuroraLookAndFeel()
{
    const auto text = juce::Colours::white.withAlpha (0.85f);

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1f2140));
    setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.12f));
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, text);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1d38));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffa78bfa).withAlpha (0.5f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, juce::Colour (0xffa78bfa).withAlpha (0.4f));
    setColour (juce::Slider::backgroundColourId, juce::Colours::white.withAlpha (0.1f));
    setColour (juce::Slider::trackColourId, juce::Colour (0xffa78bfa));
    setColour (juce::Slider::thumbColourId, juce::Colours::white);
}

juce::Font AuroraLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (14.0f));
}

juce::Font AuroraLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void AuroraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float cx = centre.x, cy = centre.y;
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    const float arcR = radius - 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Track
    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc (from the centre for bipolar controls)
    float fromAngle = startAngle;
    if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
        fromAngle = startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle);

    const float a0 = juce::jmin (fromAngle, angle);
    const float a1 = juce::jmax (fromAngle, angle);
    if (a1 - a0 > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (cx, cy, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (accent.withAlpha (0.22f)); // soft glow
        g.strokePath (value, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Knob body
    const float kr = radius - 13.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34375c), cx, cy - kr,
                                             juce::Colour (0xff14162b), cx, cy + kr, false));
    g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    // Pointer
    juce::Path p;
    p.addRoundedRectangle (-1.8f, -kr + 4.0f, 3.6f, kr * 0.42f, 1.8f);
    g.setColour (accent);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void AuroraLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? accent : juce::Colours::white.withAlpha (highlighted ? 0.14f : 0.08f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.0f : 0.14f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    g.setColour (on ? juce::Colour (0xff0d1020) : juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& name,
            const juce::String& suffix, int decimals, juce::Colour accent, double defaultValue)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (200);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    slider.setTextValueSuffix (suffix);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.setDoubleClickReturnValue (true, defaultValue); // double-click resets
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    slider.setBounds (b);
}

//==============================================================================
AuroraEditor::AuroraEditor (AuroraProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      pitch (p.apvts, "pitch", "PITCH", " st", 2, teal, 0.0),
      formant (p.apvts, "formant", "FORMANT", " st", 2, teal, 0.0),
      shiftMix (p.apvts, "shiftMix", "MIX", " %", 0, teal, 100.0),
      decay (p.apvts, "decay", "DECAY", " s", 1, violet, 2.5),
      size (p.apvts, "size", "SIZE", " %", 0, violet, 60.0),
      preDelay (p.apvts, "preDelay", "PRE-DELAY", " ms", 0, violet, 15.0),
      damp (p.apvts, "damp", "DAMP", " Hz", 0, violet, 9000.0),
      lowCut (p.apvts, "lowCut", "LOW CUT", " Hz", 0, violet, 60.0),
      mod (p.apvts, "mod", "MOD", " %", 0, violet, 30.0),
      revMix (p.apvts, "revMix", "MIX", " %", 0, violet, 25.0)
{
    setLookAndFeel (&laf);

    for (auto* k : { &pitch, &formant, &shiftMix, &decay, &size, &preDelay, &damp, &lowCut, &mod, &revMix })
        addAndMakeVisible (k);

    snapButton.setColour (juce::ToggleButton::tickColourId, teal);
    addAndMakeVisible (snapButton);
    snapAtt = std::make_unique<ButtonAtt> (p.apvts, "snap", snapButton);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
        addAndMakeVisible (l);
    };
    setupLabel (colorLabel, "COLOR");
    colorLabel.setJustificationType (juce::Justification::centred);
    setupLabel (routingLabel, "ROUTING");
    setupLabel (outputLabel, "OUTPUT");

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("color")))
        colorBox.addItemList (c->choices, 1);
    addAndMakeVisible (colorBox);
    colorAtt = std::make_unique<ComboAtt> (p.apvts, "color", colorBox);

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("order")))
        routingBox.addItemList (c->choices, 1);
    addAndMakeVisible (routingBox);
    routingAtt = std::make_unique<ComboAtt> (p.apvts, "order", routingBox);

    outputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    outputSlider.setTextValueSuffix (" dB");
    outputSlider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (outputSlider);
    outputAtt = std::make_unique<SliderAtt> (p.apvts, "output", outputSlider);
    outputSlider.setDoubleClickReturnValue (true, 0.0);

    setSize (820, 460);
}

AuroraEditor::~AuroraEditor()
{
    setLookAndFeel (nullptr);
}

void AuroraEditor::paint (juce::Graphics& g)
{
    const auto w = (float) getWidth(), h = (float) getHeight();

    // Background
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0d1d), 0.0f, 0.0f,
                                             juce::Colour (0xff1c1538), w, h, false));
    g.fillAll();

    // Soft glows behind the two panels
    g.setGradientFill (juce::ColourGradient (teal.withAlpha (0.10f), 130.0f, 120.0f,
                                             teal.withAlpha (0.0f), 130.0f, 330.0f, true));
    g.fillRect (getLocalBounds());
    g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.10f), 560.0f, 120.0f,
                                             violet.withAlpha (0.0f), 560.0f, 360.0f, true));
    g.fillRect (getLocalBounds());

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    g.drawText ("Aurora", headerArea.withTrimmedLeft (28), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("voice shifter  +  vintage space", headerArea.withTrimmedRight (28), juce::Justification::centredRight);

    // Panels
    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title, const juce::String& sub, juce::Colour accent)
    {
        const auto rf = r.toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillRoundedRectangle (rf, 18.0f);
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawRoundedRectangle (rf.reduced (0.5f), 18.0f, 1.0f);

        g.setColour (accent);
        g.fillEllipse (rf.getX() + 20.0f, rf.getY() + 21.0f, 8.0f, 8.0f);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText (title, r.getX() + 36, r.getY() + 12, 100, 26, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (sub, r.getX() + 100, r.getY() + 12, r.getWidth() - 116, 26, juce::Justification::centredRight);
    };
    drawPanel (leftPanel, "SHIFT", "pitch + formant", teal);
    drawPanel (rightPanel, "SPACE", "reverb", violet);
}

void AuroraEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (70);
    auto footer = area.removeFromBottom (56);
    area.reduce (16, 8);

    leftPanel = area.removeFromLeft (300);
    area.removeFromLeft (14);
    rightPanel = area;

    // Left panel: 2 x 2 grid
    {
        auto content = leftPanel.withTrimmedTop (44).reduced (10);
        const int cw = content.getWidth() / 2, ch = content.getHeight() / 2;
        auto cell = [&] (int row, int col)
        {
            return juce::Rectangle<int> (content.getX() + col * cw, content.getY() + row * ch, cw, ch);
        };
        pitch.setBounds (cell (0, 0));
        formant.setBounds (cell (0, 1));
        shiftMix.setBounds (cell (1, 0));
        snapButton.setBounds (cell (1, 1).withSizeKeepingCentre (118, 34));
    }

    // Right panel: 2 x 4 grid
    {
        auto content = rightPanel.withTrimmedTop (44).reduced (10);
        const int cw = content.getWidth() / 4, ch = content.getHeight() / 2;
        auto cell = [&] (int row, int col)
        {
            return juce::Rectangle<int> (content.getX() + col * cw, content.getY() + row * ch, cw, ch);
        };
        decay.setBounds (cell (0, 0));
        size.setBounds (cell (0, 1));
        preDelay.setBounds (cell (0, 2));
        damp.setBounds (cell (0, 3));
        lowCut.setBounds (cell (1, 0));
        mod.setBounds (cell (1, 1));
        revMix.setBounds (cell (1, 2));

        auto colorCell = cell (1, 3).withSizeKeepingCentre (cw - 12, 56);
        colorLabel.setBounds (colorCell.removeFromTop (20));
        colorBox.setBounds (colorCell.withTrimmedTop (4).withHeight (30));
    }

    // Footer
    {
        auto f = footer.reduced (32, 12);
        routingLabel.setBounds (f.removeFromLeft (70));
        routingBox.setBounds (f.removeFromLeft (170));
        f.removeFromLeft (40);
        outputLabel.setBounds (f.removeFromLeft (64));
        outputSlider.setBounds (f);
    }
}
