/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PFMCPP_Project10AudioProcessor::PFMCPP_Project10AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    valueTree.setProperty(juce::Identifier("Decay Time"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Average Time"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Meter View Mode"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Goniometer Scale"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Enable Hold"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Hold Time"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Histogram View"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("Peak Threshold"), 1, nullptr);
    valueTree.setProperty(juce::Identifier("RMS Threshold"), 1, nullptr);
}

PFMCPP_Project10AudioProcessor::~PFMCPP_Project10AudioProcessor()
{
}

//==============================================================================
const juce::String PFMCPP_Project10AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PFMCPP_Project10AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PFMCPP_Project10AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PFMCPP_Project10AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PFMCPP_Project10AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PFMCPP_Project10AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PFMCPP_Project10AudioProcessor::getCurrentProgram()
{
    return 0;
}

void PFMCPP_Project10AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PFMCPP_Project10AudioProcessor::getProgramName (int index)
{
    return {};
}

void PFMCPP_Project10AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PFMCPP_Project10AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    audioBufferFifo.prepare(samplesPerBlock, getNumInputChannels());



    #if OSC_GAIN
        juce::dsp::ProcessSpec oscSpec;
        oscSpec.sampleRate = sampleRate;
        oscSpec.maximumBlockSize = samplesPerBlock;
        oscSpec.numChannels = getNumInputChannels();

        osc.initialise([](float x) { return std::sin(x); });
        osc.prepare(oscSpec);
        osc.setFrequency(440.0f);

        osc2.initialise([](float x) { return std::sin(x); });
        osc2.prepare(oscSpec);
        osc2.setFrequency(440.f);

        gain.reset();
        gain.prepare(oscSpec);
        gain.setGainDecibels(-24);

        panner.reset();
        panner.prepare(oscSpec);
        panner.setRule(juce::dsp::PannerRule::sin3dB);
        panner.setPan(0.0f);
    #endif
}

void PFMCPP_Project10AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PFMCPP_Project10AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PFMCPP_Project10AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    #if OSC_GAIN
        auto numSamples = buffer.getNumSamples();
        buffer.clear();

        gain.setGainDecibels(JUCE_LIVE_CONSTANT(-6));    // gain
        //panner.setPan(JUCE_LIVE_CONSTANT(0) / 100);

        auto audioBlock = juce::dsp::AudioBlock<float>(buffer);
        auto gainProcessContext = juce::dsp::ProcessContextReplacing<float>(audioBlock);

        //osc.process(gainProcessContext);
        for (int i = 0; i < numSamples; ++i)
        {
            auto sample = osc.processSample(0);
            auto sample2 = osc2.processSample(0);
            //sample2 *= JUCE_LIVE_CONSTANT(1);
            //for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            //{
            //    buffer.setSample(channel, i, sample);
            //}
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample2);
        }

        gain.process(gainProcessContext);
        panner.process(gainProcessContext);
    #endif
    audioBufferFifo.push(buffer);
    //buffer.clear();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.

    //for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    //    buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    //for (int channel = 0; channel < totalNumInputChannels; ++channel)
    //{
    //    auto* channelData = buffer.getWritePointer (channel);
    //    // ..do something to the data...
    //}
}

//==============================================================================
bool PFMCPP_Project10AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PFMCPP_Project10AudioProcessor::createEditor()
{
    return new PFMCPP_Project10AudioProcessorEditor (*this);
}

//==============================================================================
void PFMCPP_Project10AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    if (valueTree.isValid())
    {
        juce::MemoryOutputStream mos(destData, false);
        valueTree.writeToStream(mos);
    }
}

void PFMCPP_Project10AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::MemoryBlock mb(data, sizeInBytes);
    juce::MemoryInputStream mis(mb, false);
    juce::ValueTree loadTree = juce::ValueTree::readFromStream(mis);

    if (valueTree.isValid() && valueTree.isEquivalentTo(loadTree))
    {
        valueTree = loadTree;

    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PFMCPP_Project10AudioProcessor();
}
