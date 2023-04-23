/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define OSC_GAIN true
//==============================================================================
/**
*/
template<typename T>
struct Averager
{
    Averager(size_t numElements, T initialValue)
    {
        resize(numElements, initialValue);
    }

    void resize(size_t numElements, T initialValue)
    {
        elements.resize(numElements);
        clear(initialValue);
    }

    void clear(T initialValue)
    {
        for (auto& element : elements)
        {
            element = initialValue;
        }
        
        writeIndex = 0;
        sum = initialValue * elements.size();
        avg = static_cast<float>(initialValue);
    }

    size_t getSize() const { return elements.size(); }

    void add(T t)
    {
        auto currentSum = sum.load();
        auto currentIndex = writeIndex.load();
        auto currentAvg = avg.load();

        currentSum = currentSum - elements[currentIndex] + t;
        currentAvg = static_cast<float>(currentSum / elements.size());
        elements[currentIndex] = t;
        ++currentIndex;
        if (currentIndex == elements.size())
            currentIndex = 0;

        sum.store(currentSum);
        writeIndex.store(currentIndex);
        avg.store(currentAvg);
    }

    float getAvg() const { return avg; }

private:
    std::vector<T> elements;
    std::atomic<float> avg{ static_cast<float>(T()) };
    std::atomic<size_t> writeIndex = 0;
    std::atomic<T> sum{ 0 };
};
//==============================================================================
template<typename T, size_t Size>
struct Fifo
{
    size_t getSize() const noexcept
    {
        return Size;
    }

    void prepare(int numSamples, int numChannels)
    {
        for (auto& bufferCell : buffer)
        {
            // setSize() and clear() are taken from AudioBuffer<Type> class
            bufferCell.setSize(numChannels,
                       numSamples,
                       false,
                       true,
                       false);

            bufferCell.clear();
        }
    }

    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffer[write.startIndex1] = t;
            return true;
        }
        return false;
    }
    bool pull(T& t)
    {   
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffer[read.startIndex1];
            return true;
        }
        return false;
    }

    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
    int getAvailableSpace() const
    {
        return fifo.getFreeSpace();
    }

private:
    juce::AbstractFifo fifo{ Size };
    std::array<T, Size> buffer;
};

class PFMCPP_Project10AudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    PFMCPP_Project10AudioProcessor();
    ~PFMCPP_Project10AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    Fifo<juce::AudioBuffer<float>, 32> audioBufferFifo;

private:
    #if OSC_GAIN
        juce::dsp::Oscillator<float> osc;
        juce::dsp::Gain<float> gain;
    #endif
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFMCPP_Project10AudioProcessor)
};
