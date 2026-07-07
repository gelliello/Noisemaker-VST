#pragma once

#include <JuceHeader.h>

class PitchDetector
{
public:
    void prepare (double newSampleRate, int maximumBlockSize);
    void reset();

    float processBlock (const juce::AudioBuffer<float>& input, float sensitivity);
    bool isNoteActive() const noexcept { return noteActive; }
    float getConfidence() const noexcept { return confidence; }

private:
    float estimatePitchYin();

    double sampleRate = 44100.0;
    int analysisSize = 2048;
    int writePosition = 0;
    bool bufferFilled = false;
    bool noteActive = false;
    float confidence = 0.0f;
    float smoothedPitch = 0.0f;

    juce::AudioBuffer<float> ringBuffer;
    std::vector<float> analysisBuffer;
    std::vector<float> yinBuffer;
};
