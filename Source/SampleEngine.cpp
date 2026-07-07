#include "SampleEngine.h"
#include "PitchDetector.h"

void SampleEngine::prepare (double newSampleRate)
{
    hostSampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void SampleEngine::reset()
{
    position = 0.0;
    increment = 1.0;
    playing = false;
    previousNoteActive = false;
    envelope = 0.0f;
}

void SampleEngine::setSample (std::shared_ptr<const LoadedSample> newSample)
{
    const juce::SpinLock::ScopedLockType lock (sampleMutex);
    pendingSample = std::move (newSample);
}

void SampleEngine::render (juce::AudioBuffer<float>& output,
                           float targetFrequency,
                           bool noteActive,
                           float gain,
                           float mix,
                           float pitchOffsetSemitones,
                           float attackMs,
                           float releaseMs,
                           Mode mode)
{
    {
        const juce::SpinLock::ScopedTryLockType lock (sampleMutex);
        if (lock.isLocked() && pendingSample != nullptr)
        {
            sample = pendingSample;
            pendingSample.reset();
            reset();
        }
    }

    if (sample == nullptr || sample->audio.getNumSamples() == 0)
        return;

    const bool trigger = noteActive && ! previousNoteActive;
    previousNoteActive = noteActive;

    if (trigger)
    {
        position = 0.0;
        playing = true;
    }

    if (mode == Mode::sustain && ! noteActive)
        playing = false;

    if (! playing && envelope <= 0.0001f)
        return;

    const auto root = juce::jmax (20.0f, sample->rootFrequency);
    const auto target = juce::jmax (20.0f, targetFrequency);
    const auto pitchRatio = target / root * std::pow (2.0f, pitchOffsetSemitones / 12.0f);
    increment = (sample->sampleRate / hostSampleRate) * pitchRatio;

    const auto attackSamples = juce::jmax (1.0, attackMs * 0.001 * hostSampleRate);
    const auto releaseSamples = juce::jmax (1.0, releaseMs * 0.001 * hostSampleRate);
    const auto attackStep = (float) (1.0 / attackSamples);
    const auto releaseStep = (float) (1.0 / releaseSamples);
    const auto wetGain = juce::Decibels::decibelsToGain (gain) * mix;

    for (int i = 0; i < output.getNumSamples(); ++i)
    {
        const bool shouldRise = playing && (mode == Mode::oneShot || noteActive);
        envelope += shouldRise ? attackStep : -releaseStep;
        envelope = juce::jlimit (0.0f, 1.0f, envelope);

        if (envelope <= 0.0001f && ! playing)
            break;

        if (position >= sample->audio.getNumSamples() - 3)
        {
            playing = false;
            if (mode == Mode::oneShot)
                envelope = juce::jmax (0.0f, envelope - releaseStep);
            continue;
        }

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addSample (ch, i, readInterpolated (ch % sample->audio.getNumChannels(), position) * envelope * wetGain);

        position += increment;
    }
}

float SampleEngine::readInterpolated (int channel, double readPosition) const
{
    const auto& buffer = sample->audio;
    const int i1 = juce::jlimit (0, buffer.getNumSamples() - 1, (int) readPosition);
    const int i0 = juce::jmax (0, i1 - 1);
    const int i2 = juce::jmin (buffer.getNumSamples() - 1, i1 + 1);
    const int i3 = juce::jmin (buffer.getNumSamples() - 1, i1 + 2);
    const auto frac = (float) (readPosition - i1);

    const auto y0 = buffer.getSample (channel, i0);
    const auto y1 = buffer.getSample (channel, i1);
    const auto y2 = buffer.getSample (channel, i2);
    const auto y3 = buffer.getSample (channel, i3);

    const auto a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const auto a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const auto a2 = -0.5f * y0 + 0.5f * y2;
    return ((a0 * frac + a1) * frac + a2) * frac + y1;
}

SampleLoader::SampleLoader()
{
    formatManager.registerBasicFormats();
}

std::shared_ptr<LoadedSample> SampleLoader::loadFile (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File does not exist.";
        return {};
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or invalid audio file.";
        return {};
    }

    if (reader->lengthInSamples <= 0 || reader->numChannels == 0)
    {
        error = "The audio file is empty.";
        return {};
    }

    auto loaded = std::make_shared<LoadedSample>();
    const auto channelsToRead = juce::jmin ((int) reader->numChannels, 2);
    const auto maximumSamples = (int64_t) std::round (reader->sampleRate * 300.0);
    const auto samplesToRead = (int) juce::jmin ((int64_t) reader->lengthInSamples, maximumSamples);
    loaded->audio.setSize (channelsToRead, samplesToRead);
    reader->read (&loaded->audio, 0, loaded->audio.getNumSamples(), 0, true, true);

    removeLeadingSilence (loaded->audio);
    loaded->sampleRate = reader->sampleRate;
    loaded->rootFrequency = estimateRootFrequency (loaded->audio, loaded->sampleRate);
    loaded->fileName = file.getFileName();
    error.clear();
    return loaded;
}

void SampleLoader::removeLeadingSilence (juce::AudioBuffer<float>& buffer)
{
    const auto threshold = juce::Decibels::decibelsToGain (-55.0f);
    int firstSound = 0;

    for (; firstSound < buffer.getNumSamples(); ++firstSound)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, firstSound)));

        if (peak > threshold)
            break;
    }

    if (firstSound <= 0 || firstSound >= buffer.getNumSamples())
        return;

    juce::AudioBuffer<float> trimmed (buffer.getNumChannels(), buffer.getNumSamples() - firstSound);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        trimmed.copyFrom (ch, 0, buffer, ch, firstSound, trimmed.getNumSamples());

    buffer = std::move (trimmed);
}

float SampleLoader::estimateRootFrequency (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumSamples() < 2048)
        return 261.6256f;

    PitchDetector detector;
    detector.prepare (sampleRate, 512);

    juce::AudioBuffer<float> mono (1, juce::jmin (buffer.getNumSamples(), (int) sampleRate * 2));
    mono.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        mono.addFrom (0, 0, buffer, ch, 0, mono.getNumSamples(), 1.0f / buffer.getNumChannels());

    float bestPitch = 0.0f;
    float bestConfidence = 0.0f;
    constexpr int hop = 512;
    juce::AudioBuffer<float> block (1, hop);

    for (int pos = 0; pos + hop <= mono.getNumSamples(); pos += hop)
    {
        block.copyFrom (0, 0, mono, 0, pos, hop);
        const auto pitch = detector.processBlock (block, 0.75f);
        if (detector.getConfidence() > bestConfidence && pitch > 30.0f)
        {
            bestConfidence = detector.getConfidence();
            bestPitch = pitch;
        }
    }

    return bestPitch > 0.0f ? bestPitch : 261.6256f;
}
