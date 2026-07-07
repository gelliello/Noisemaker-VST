#include "PluginEditor.h"
#include "BinaryData.h"

DropArea::DropArea (NoisemakerAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    setRepaintsOnMouseActivity (true);
}

void DropArea::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (hovering ? juce::Colour (0xff35d0ff) : juce::Colour (0xff2d3647));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colour (0xff6df2a6));
    g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);

    auto icon = juce::Drawable::createFromImageData (BinaryData::noisemaker_icon_svg,
                                                     BinaryData::noisemaker_icon_svgSize);
    if (icon != nullptr)
        icon->drawWithin (g, getLocalBounds().removeFromTop (92).toFloat().reduced (18.0f),
                          juce::RectanglePlacement::centred, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawFittedText ("Drop Audio Here", getLocalBounds().withTrimmedTop (94), juce::Justification::centred, 1);
}

void DropArea::mouseUp (const juce::MouseEvent&)
{
    auto chooser = std::make_shared<juce::FileChooser> ("Load audio file",
                                                        juce::File{},
                                                        "*.wav;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile())
                processor.loadSampleAsync (fc.getResult());
        });
}

bool DropArea::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const auto ext = juce::File (path).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
            return true;
    }
    return false;
}

void DropArea::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;
    for (const auto& path : files)
    {
        const auto file = juce::File (path);
        if (file.existsAsFile())
        {
            processor.loadSampleAsync (file);
            break;
        }
    }
}

NoisemakerAudioProcessorEditor::NoisemakerAudioProcessorEditor (NoisemakerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), dropArea (p)
{
    setSize (760, 520);

    titleLabel.setText ("Noisemaker", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    fileLabel.setColour (juce::Label::textColourId, juce::Colour (0xffdbe7ff));
    errorLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff7777));
    pitchLabel.setColour (juce::Label::textColourId, juce::Colour (0xff6df2a6));
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (errorLabel);
    addAndMakeVisible (pitchLabel);
    addAndMakeVisible (dropArea);

    configureSlider (volumeSlider, volumeLabel, "Volume");
    configureSlider (mixSlider, mixLabel, "Wet/Dry");
    configureSlider (offsetSlider, offsetLabel, "Pitch Offset");
    configureSlider (attackSlider, attackLabel, "Attack");
    configureSlider (releaseSlider, releaseLabel, "Release");
    configureSlider (sensitivitySlider, sensitivityLabel, "Sensitivity");

    modeBox.addItem ("One-Shot", 1);
    modeBox.addItem ("Sustain", 2);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    volumeAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "volume", volumeSlider);
    mixAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "mix", mixSlider);
    offsetAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "offset", offsetSlider);
    attackAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "release", releaseSlider);
    sensitivityAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "sensitivity", sensitivitySlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (audioProcessor.parameters, "bypass", bypassButton);
    modeAttachment = std::make_unique<ComboAttachment> (audioProcessor.parameters, "mode", modeBox);

    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };

    startTimerHz (20);
}

void NoisemakerAudioProcessorEditor::configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 22);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff6df2a6));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff35d0ff));
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void NoisemakerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111823));
    g.setColour (juce::Colour (0xff1b2635));
    g.fillRect (getLocalBounds().removeFromTop (74));
}

void NoisemakerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (22);
    auto header = area.removeFromTop (52);
    titleLabel.setBounds (header.removeFromLeft (280));
    pitchLabel.setBounds (header.removeFromRight (180));
    bypassButton.setBounds (header.removeFromRight (100).reduced (6));

    auto left = area.removeFromLeft (330);
    dropArea.setBounds (left.removeFromTop (220));
    fileLabel.setBounds (left.removeFromTop (30));
    errorLabel.setBounds (left.removeFromTop (30));

    auto buttons = left.removeFromTop (42);
    loadButton.setBounds (buttons.removeFromLeft (140).reduced (4));
    saveButton.setBounds (buttons.removeFromLeft (140).reduced (4));
    modeBox.setBounds (left.removeFromTop (34).reduced (4));

    area.removeFromLeft (22);
    auto grid = area;
    const int columnWidth = grid.getWidth() / 3;
    const int rowHeight = 190;
    juce::Array<juce::Component*> sliders { &volumeSlider, &mixSlider, &offsetSlider, &attackSlider, &releaseSlider, &sensitivitySlider };
    juce::Array<juce::Component*> labels { &volumeLabel, &mixLabel, &offsetLabel, &attackLabel, &releaseLabel, &sensitivityLabel };

    for (int i = 0; i < sliders.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (grid.getX() + (i % 3) * columnWidth,
                                          grid.getY() + (i / 3) * rowHeight,
                                          columnWidth,
                                          rowHeight).reduced (8);
        labels[i]->setBounds (cell.removeFromTop (24));
        sliders[i]->setBounds (cell);
    }
}

void NoisemakerAudioProcessorEditor::timerCallback()
{
    fileLabel.setText ("Loaded: " + audioProcessor.getLoadedFileName(), juce::dontSendNotification);
    errorLabel.setText (audioProcessor.getLastError(), juce::dontSendNotification);

    const auto pitch = audioProcessor.getCurrentPitchHz();
    pitchLabel.setText (pitch > 0.0f ? juce::String (pitch, 1) + " Hz" : "No pitch",
                        juce::dontSendNotification);
}

void NoisemakerAudioProcessorEditor::savePreset()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Save Noisemaker preset",
                                                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                            .getChildFile ("Noisemaker.nmkp"),
                                                        "*.nmkp");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File{})
                audioProcessor.savePresetToFile (fc.getResult().withFileExtension (".nmkp"));
        });
}

void NoisemakerAudioProcessorEditor::loadPreset()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Load Noisemaker preset",
                                                        juce::File{},
                                                        "*.nmkp");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile())
                audioProcessor.loadPresetFromFile (fc.getResult());
        });
}
