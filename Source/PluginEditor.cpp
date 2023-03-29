/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PFMCPP_Project10AudioProcessorEditor::PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    startTimerHz(60);
    setSize (400, 300);
}

PFMCPP_Project10AudioProcessorEditor::~PFMCPP_Project10AudioProcessorEditor()
{
}

//==============================================================================
void PFMCPP_Project10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void Meter::paint(juce::Graphics& g)
{
    using namespace juce;

    auto bounds = getLocalBounds().toFloat().reduced(10);
    g.setColour(Colours::darkgrey);
    g.drawRect(bounds, 10.0f);
    float remappedPeakDb = jmap<float>(peakDb, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getHeight(), 0.0f);
    g.setColour(Colours::white);
    g.fillRect(0.0f, remappedPeakDb, bounds.reduced(6).getWidth(), bounds.reduced(6).getHeight());
}

void Meter::update(float dbLevel)
{
    peakDb = dbLevel;
    repaint();
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
