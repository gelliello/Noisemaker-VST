#pragma once

#include <JuceHeader.h>

struct LoadedSample
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    float rootFrequency = 261.6256f;
    juce::String fileName;
};

class SampleEngine
{
public:
    enum class Mode
    {
        oneShot,
        sustain
    };

    void prepare (double newSampleRate);
    void reset();
    void setSample (std::shared_ptr<const LoadedSample> newSample);
    void render (juce::AudioBuffer<float>& output,
                 float targetFrequency,
                 bool noteActive,
                 float gain,
                 float mix,
                 float pitchOffsetSemitones,
                 float attackMs,
                 float releaseMs,
                 Mode mode);

private:
    float readInterpolated (int channel, double position) const;

    std::shared_ptr<const LoadedSample> sample;
    std::shared_ptr<const LoadedSample> pendingSample;
    juce::SpinLock sampleMutex;
    double hostSampleRate = 44100.0;
    double position = 0.0;
    double increment = 1.0;
    bool playing = false;
    bool previousNoteActive = false;
    float envelope = 0.0f;
};

class SampleLoader
{
public:
    SampleLoader();
    std::shared_ptr<LoadedSample> loadFile (const juce::File& file, juce::String& error);

private:
    static void removeLeadingSilence (juce::AudioBuffer<float>& buffer);
    static float estimateRootFrequency (const juce::AudioBuffer<float>& buffer, double sampleRate);

    juce::AudioFormatManager formatManager;
};
