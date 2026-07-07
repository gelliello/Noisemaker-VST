#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DropArea final : public juce::Component,
                       public juce::FileDragAndDropTarget
{
public:
    explicit DropArea (NoisemakerAudioProcessor& processorToUse);
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void setHovering (bool shouldHover) { hovering = shouldHover; repaint(); }
    void fileDragEnter (const juce::StringArray&, int, int) override { setHovering (true); }
    void fileDragExit (const juce::StringArray&) override { setHovering (false); }

private:
    NoisemakerAudioProcessor& processor;
    bool hovering = false;
};

class NoisemakerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit NoisemakerAudioProcessorEditor (NoisemakerAudioProcessor&);
    ~NoisemakerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void savePreset();
    void loadPreset();

    NoisemakerAudioProcessor& audioProcessor;
    DropArea dropArea;

    juce::Label titleLabel;
    juce::Label fileLabel;
    juce::Label errorLabel;
    juce::Label pitchLabel;

    juce::Slider volumeSlider;
    juce::Slider mixSlider;
    juce::Slider offsetSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider sensitivitySlider;

    juce::Label volumeLabel;
    juce::Label mixLabel;
    juce::Label offsetLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label sensitivityLabel;

    juce::ToggleButton bypassButton { "Bypass" };
    juce::ComboBox modeBox;
    juce::TextButton saveButton { "Save Preset" };
    juce::TextButton loadButton { "Load Preset" };

    std::unique_ptr<SliderAttachment> volumeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> offsetAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> sensitivityAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoisemakerAudioProcessorEditor)
};
