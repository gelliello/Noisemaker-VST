#include "PluginProcessor.h"
#include "PluginEditor.h"

NoisemakerAudioProcessor::NoisemakerAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
}

NoisemakerAudioProcessor::~NoisemakerAudioProcessor()
{
    callbackAlive->store (false);

    if (loadThread != nullptr)
        loadThread->stopThread (3000);
}

juce::AudioProcessorValueTreeState::ParameterLayout NoisemakerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("volume", "Volume",
        juce::NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Wet/Dry",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("offset", "Pitch Offset",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack", "Attack",
        juce::NormalisableRange<float> (0.1f, 250.0f, 0.1f, 0.45f), 2.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release",
        juce::NormalisableRange<float> (5.0f, 1500.0f, 0.1f, 0.45f), 140.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sensitivity", "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.72f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode",
        juce::StringArray { "One-Shot", "Sustain" }, 0));

    return { params.begin(), params.end() };
}

void NoisemakerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pitchDetector.prepare (sampleRate, samplesPerBlock);
    sampleEngine.prepare (sampleRate);
}

void NoisemakerAudioProcessor::releaseResources()
{
    sampleEngine.reset();
    pitchDetector.reset();
}

bool NoisemakerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    return (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo())
        && (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo())
        && mainIn.size() <= mainOut.size();
}

void NoisemakerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto bypassed = parameters.getRawParameterValue ("bypass")->load() > 0.5f;
    const auto sensitivity = parameters.getRawParameterValue ("sensitivity")->load();
    const auto detectedPitch = pitchDetector.processBlock (buffer, sensitivity);
    currentPitchHz.store (detectedPitch);

    if (bypassed)
        return;

    const auto mix = parameters.getRawParameterValue ("mix")->load();
    buffer.applyGain (1.0f - mix);

    if (detectedPitch > 0.0f && pitchDetector.isNoteActive())
    {
        sampleEngine.render (buffer,
                             detectedPitch,
                             true,
                             parameters.getRawParameterValue ("volume")->load(),
                             mix,
                             parameters.getRawParameterValue ("offset")->load(),
                             parameters.getRawParameterValue ("attack")->load(),
                             parameters.getRawParameterValue ("release")->load(),
                             (int) parameters.getRawParameterValue ("mode")->load() == 0
                                ? SampleEngine::Mode::oneShot
                                : SampleEngine::Mode::sustain);
    }
    else
    {
        sampleEngine.render (buffer,
                             juce::jmax (20.0f, currentPitchHz.load()),
                             false,
                             parameters.getRawParameterValue ("volume")->load(),
                             mix,
                             parameters.getRawParameterValue ("offset")->load(),
                             parameters.getRawParameterValue ("attack")->load(),
                             parameters.getRawParameterValue ("release")->load(),
                             (int) parameters.getRawParameterValue ("mode")->load() == 0
                                ? SampleEngine::Mode::oneShot
                                : SampleEngine::Mode::sustain);
    }
}

void NoisemakerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("loadedFileName", loadedFileName, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NoisemakerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        loadedFileName = tree.getProperty ("loadedFileName", "No file loaded").toString();
        parameters.replaceState (tree);
    }
}

void NoisemakerAudioProcessor::loadSampleAsync (const juce::File& file)
{
    if (loadThread != nullptr)
        loadThread->stopThread (3000);

    loadThread = std::make_unique<LoadThread> (*this, file);
    loadThread->startThread();
}

NoisemakerAudioProcessor::LoadThread::LoadThread (NoisemakerAudioProcessor& ownerToUse, juce::File fileToLoad)
    : Thread ("Noisemaker sample loader"), owner (ownerToUse), file (std::move (fileToLoad))
{
}

void NoisemakerAudioProcessor::LoadThread::run()
{
    juce::String error;
    auto loaded = owner.sampleLoader.loadFile (file, error);

    if (threadShouldExit())
        return;

    auto alive = owner.callbackAlive;
    auto* ownerPtr = &owner;

    juce::MessageManager::callAsync ([alive, ownerPtr, loaded, error]
    {
        if (! alive->load())
            return;

        if (loaded != nullptr)
            ownerPtr->applyLoadedSample (loaded);
        else
            ownerPtr->setLastError (error);
    });
}

void NoisemakerAudioProcessor::applyLoadedSample (std::shared_ptr<LoadedSample> loaded)
{
    const juce::ScopedLock lock (sampleLock);
    currentSample = loaded;
    loadedFileName = loaded->fileName;
    lastError.clear();
    sampleEngine.setSample (currentSample);
}

void NoisemakerAudioProcessor::setLastError (juce::String message)
{
    lastError = message;
}

juce::String NoisemakerAudioProcessor::getLoadedFileName() const
{
    return loadedFileName;
}

juce::String NoisemakerAudioProcessor::getLastError() const
{
    return lastError;
}

void NoisemakerAudioProcessor::savePresetToFile (const juce::File& file)
{
    juce::MemoryBlock block;
    getStateInformation (block);
    file.replaceWithData (block.getData(), block.getSize());
}

void NoisemakerAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    juce::MemoryBlock block;
    if (file.loadFileAsData (block))
        setStateInformation (block.getData(), (int) block.getSize());
}

juce::AudioProcessorEditor* NoisemakerAudioProcessor::createEditor()
{
    return new NoisemakerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoisemakerAudioProcessor();
}
