/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class PFMCPP_Project10AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              public juce::Timer
{
public:
    PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor&);
    ~PFMCPP_Project10AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PFMCPP_Project10AudioProcessor& audioProcessor;

    juce::AudioBuffer<float> audioBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFMCPP_Project10AudioProcessorEditor)
};
