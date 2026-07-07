#include "PitchDetector.h"

void PitchDetector::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    analysisSize = juce::jlimit (1024, 4096, (int) std::round (sampleRate * 0.046));

    ringBuffer.setSize (1, analysisSize);
    ringBuffer.clear();
    analysisBuffer.assign ((size_t) analysisSize, 0.0f);
    yinBuffer.assign ((size_t) analysisSize / 2, 0.0f);
    reset();
}

void PitchDetector::reset()
{
    writePosition = 0;
    bufferFilled = false;
    noteActive = false;
    confidence = 0.0f;
    smoothedPitch = 0.0f;
    ringBuffer.clear();
}

float PitchDetector::processBlock (const juce::AudioBuffer<float>& input, float sensitivity)
{
    if (input.getNumSamples() == 0 || input.getNumChannels() == 0)
        return 0.0f;

    const auto* channel = input.getReadPointer (0);
    auto rms = input.getRMSLevel (0, 0, input.getNumSamples());
    const auto gate = juce::jmap (sensitivity, 0.0f, 1.0f, 0.045f, 0.004f);

    for (int i = 0; i < input.getNumSamples(); ++i)
    {
        ringBuffer.setSample (0, writePosition, channel[i]);
        writePosition = (writePosition + 1) % analysisSize;
        if (writePosition == 0)
            bufferFilled = true;
    }

    if (! bufferFilled || rms < gate)
    {
        noteActive = false;
        confidence = 0.0f;
        return 0.0f;
    }

    const auto pitch = estimatePitchYin();
    noteActive = pitch > 0.0f && confidence > juce::jmap (sensitivity, 0.0f, 1.0f, 0.90f, 0.62f);

    if (! noteActive)
        return 0.0f;

    smoothedPitch = smoothedPitch <= 0.0f ? pitch : (0.72f * smoothedPitch + 0.28f * pitch);
    return smoothedPitch;
}

float PitchDetector::estimatePitchYin()
{
    for (int i = 0; i < analysisSize; ++i)
        analysisBuffer[(size_t) i] = ringBuffer.getSample (0, (writePosition + i) % analysisSize);

    const int tauMax = (int) yinBuffer.size() - 1;
    const int tauMin = juce::jmax (2, (int) std::floor (sampleRate / 1100.0));
    const int guitarLowTau = juce::jmin (tauMax - 1, (int) std::ceil (sampleRate / 55.0));

    std::fill (yinBuffer.begin(), yinBuffer.end(), 0.0f);

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        const int limit = analysisSize - tau;
        for (int i = 0; i < limit; ++i)
        {
            const auto delta = analysisBuffer[(size_t) i] - analysisBuffer[(size_t) (i + tau)];
            sum += delta * delta;
        }
        yinBuffer[(size_t) tau] = (float) sum;
    }

    yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        runningSum += yinBuffer[(size_t) tau];
        yinBuffer[(size_t) tau] = runningSum > 0.0f ? yinBuffer[(size_t) tau] * tau / runningSum : 1.0f;
    }

    const float threshold = 0.14f;
    int tauEstimate = -1;
    for (int tau = tauMin; tau <= guitarLowTau; ++tau)
    {
        if (yinBuffer[(size_t) tau] < threshold)
        {
            while (tau + 1 <= guitarLowTau && yinBuffer[(size_t) (tau + 1)] < yinBuffer[(size_t) tau])
                ++tau;
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate < 0)
    {
        confidence = 0.0f;
        return 0.0f;
    }

    auto betterTau = (float) tauEstimate;
    if (tauEstimate > 0 && tauEstimate < tauMax)
    {
        const auto s0 = yinBuffer[(size_t) (tauEstimate - 1)];
        const auto s1 = yinBuffer[(size_t) tauEstimate];
        const auto s2 = yinBuffer[(size_t) (tauEstimate + 1)];
        const auto divisor = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs (divisor) > 1.0e-9f)
            betterTau += (s2 - s0) / divisor;
    }

    confidence = juce::jlimit (0.0f, 1.0f, 1.0f - yinBuffer[(size_t) tauEstimate]);
    return betterTau > 0.0f ? (float) sampleRate / betterTau : 0.0f;
}
