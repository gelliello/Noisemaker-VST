#pragma once

#include <JuceHeader.h>
#include "PitchDetector.h"
#include "SampleEngine.h"

class NoisemakerAudioProcessor final : public juce::AudioProcessor
{
public:
    NoisemakerAudioProcessor();
    ~NoisemakerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;

    void loadSampleAsync (const juce::File& file);
    void savePresetToFile (const juce::File& file);
    void loadPresetFromFile (const juce::File& file);

    juce::String getLoadedFileName() const;
    juce::String getLastError() const;
    float getCurrentPitchHz() const noexcept { return currentPitchHz.load(); }

private:
    class LoadThread final : public juce::Thread
    {
    public:
        LoadThread (NoisemakerAudioProcessor& ownerToUse, juce::File fileToLoad);
        void run() override;

    private:
        NoisemakerAudioProcessor& owner;
        juce::File file;
    };

    void applyLoadedSample (std::shared_ptr<LoadedSample> loaded);
    void setLastError (juce::String message);

    PitchDetector pitchDetector;
    SampleEngine sampleEngine;
    SampleLoader sampleLoader;

    juce::CriticalSection sampleLock;
    std::shared_ptr<const LoadedSample> currentSample;
    juce::String loadedFileName = "No file loaded";
    juce::String lastError;
    std::atomic<float> currentPitchHz { 0.0f };
    std::shared_ptr<std::atomic_bool> callbackAlive { std::make_shared<std::atomic_bool> (true) };
    std::unique_ptr<LoadThread> loadThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoisemakerAudioProcessor)
};
